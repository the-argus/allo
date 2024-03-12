#pragma once
#include "allo/detail/abstracts.h"
#include <ziglike/stdmem.h>

#ifndef ALLO_DISABLE_TYPEINFO
#ifndef ALLO_USE_RTTI
#include "ctti/typename.h"
#endif
#endif

namespace allo {
/// Change the size of an allocation without invalidating pointers to that
/// allocation. Often fails, since it will not succeed if you request a larger
/// size and there is no way for the allocator to grow the existing allocation.
template <typename T, typename Allocator>
inline zl::res<zl::slice<T>, AllocationStatusCode>
remap(Allocator &allocator, zl::slice<T> original, size_t new_size) noexcept
{
    static_assert(!std::is_same_v<Allocator, c_allocator_t>,
                  "The c allocator does not provide remap functionality, you "
                  "must use realloc.");
    static_assert(
        detail::can_upcast<Allocator,
                           detail::dynamic_stack_allocator_t>::type::value,
        "Cannot use given type to perform reallocations");

#if defined(ALLO_DISABLE_TYPEINFO)
    const size_t typehash = 0;
#elif defined(ALLO_USE_RTTI)
    const size_t typehash = typeid(T).hash_code();
#else
    constexpr size_t typehash = ctti::nameof<T>().hash();
#endif

    size_t actual_typehash = typehash;
    if constexpr (std::is_same_v<T, uint8_t>) {
        actual_typehash = 0;
    }

    auto remapped = allocator.remap_bytes(
        zl::raw_slice(*reinterpret_cast<uint8_t *>(original.data()),
                      original.size() * sizeof(T)),
        actual_typehash, new_size * sizeof(T), actual_typehash);
    if (!remapped.okay())
        return remapped.err();

    zl::slice<uint8_t> &newmem = remapped.release_ref();
    assert(newmem.data() == (uint8_t *)original.data());

    return zl::raw_slice(*original.data(), new_size);
}

/// Remap, or if remap fails, create an entirely new, differently sized
/// allocation and copy the contents of the first allocation to that one.
template <typename Allocator>
inline zl::res<zl::slice<uint8_t>, AllocationStatusCode>
realloc_bytes(Allocator &allocator, zl::slice<uint8_t> original,
              size_t new_size) noexcept
{
    static_assert(
        detail::can_upcast<Allocator,
                           detail::dynamic_stack_allocator_t>::type::value,
        "Cannot use given type to perform reallocations");

    // necessary to perform downcasting here in order to support the C allocator
    // which breaks the abstraction
    // NOTE: this could be achieved with a ternary operator but not doing that
    // in favor of constexpr if. maybe a lambda could be okay too
    if constexpr (std::is_base_of_v<detail::dynamic_allocator_base_t,
                                    Allocator>) {
        auto *type = reinterpret_cast<detail::AllocatorType *>(
            std::addressof(allocator));
        assert(*type < detail::AllocatorType::MAX_ALLOCATOR_TYPE);
        if (*type == detail::AllocatorType::CAllocator) {
            return reinterpret_cast<c_allocator_t *>(std::addressof(allocator))
                ->realloc_bytes(original, 0, new_size, 0);
        }
    } else if constexpr (std::is_base_of_v<detail::allocator_common_t,
                                           Allocator>) {
        // NOTE: this relies on the layout of everything that an allocator
        // dynref points at always having an AllocatorType at the beginning
        auto *type = reinterpret_cast<detail::AllocatorType *>(allocator.ref);
        assert(*type < detail::AllocatorType::MAX_ALLOCATOR_TYPE);
        if (*type == detail::AllocatorType::CAllocator) {
            return reinterpret_cast<c_allocator_t *>(std::addressof(allocator))
                ->realloc_bytes(original, 0, new_size, 0);
        }
    }

    auto remap = allocator.remap_bytes(original, 0, new_size, 0);
    if (remap.okay())
        return remap;

    static_assert(sizeof(original.data()) == sizeof(size_t));
    auto new_allocation =
        allocator.alloc_bytes(new_size,
                              detail::nearest_alignment_exponent(
                                  reinterpret_cast<size_t>(original.data())),
                              0);
    if (new_allocation.okay()) {
        const bool enlarging = original.size() < new_size;
        const zl::slice<uint8_t> source =
            enlarging ? original : zl::slice<uint8_t>(original, 0, new_size);
        zl::slice<uint8_t> dest =
            enlarging ? zl::slice<uint8_t>(new_allocation.release_ref(), 0,
                                           original.size())
                      : new_allocation.release_ref();
        const bool status = zl::memcopy(dest, source);
        assert(status);
        allocator.free_bytes(original, 0);
        // TODO: assert or something here maybe? should we allow failed
        // frees? probably a warning log message would be good
        return dest;
    }
    return new_allocation.err();
}

/// Remap, or if remap fails, create an entirely new, differently sized
/// allocation and copy the contents of the first allocation to that one.
template <typename T, typename Allocator>
inline zl::res<zl::slice<T>, AllocationStatusCode>
realloc(Allocator &allocator, zl::slice<T> original, size_t new_size) noexcept
{
    static_assert(
        detail::can_upcast<Allocator,
                           detail::dynamic_stack_allocator_t>::type::value,
        "Cannot use given type to perform reallocations");

    zl::slice<uint8_t> original_bytes =
        zl::raw_slice(*reinterpret_cast<uint8_t *>(original.data()),
                      original.size() * sizeof(T));

#if defined(ALLO_DISABLE_TYPEINFO)
    const size_t typehash = 0;
#elif defined(ALLO_USE_RTTI)
    const size_t typehash = typeid(T).hash_code();
#else
    constexpr size_t typehash = ctti::nameof<T>().hash();
#endif
    // no need to set typehash to 0 if T == uint8_t, we have template
    // specialization for that.

    const size_t new_size_bytes = new_size * sizeof(T);

    auto remap = allocator.remap_bytes(original_bytes, typehash, new_size_bytes,
                                       typehash);
    if (remap.okay())
        return zl::raw_slice(*original.data(), new_size);

    static_assert(sizeof(original.data()) == sizeof(size_t));
    auto new_allocation =
        allocator.alloc_bytes(new_size_bytes,
                              detail::nearest_alignment_exponent(
                                  reinterpret_cast<size_t>(original.data())),
                              typehash);
    if (new_allocation.okay()) {
        const bool enlarging = original.size() < new_size;
        const zl::slice<T> source =
            enlarging ? original : zl::slice<T>(original, 0, new_size);
        zl::slice<T> dest = zl::raw_slice<T>(
            *reinterpret_cast<T *>(new_allocation.release_ref().data()),
            enlarging ? original.size() : new_size);
        const bool status = zl::memcopy(dest, source);
        assert(status);
        allocator.free_bytes(original_bytes, typehash);
        // TODO: assert or something here maybe? should we allow failed frees?
        // probably a warning log message would be good
        return dest;
    }
    return new_allocation.err();
}

/// When calling realloc with T = uint8_t, it just calls realloc_bytes.
template <typename Allocator>
inline zl::res<zl::slice<uint8_t>, AllocationStatusCode>
realloc(Allocator &allocator, zl::slice<uint8_t> original,
        size_t new_size) noexcept
{
    return realloc_bytes(allocator, original, new_size);
}
} // namespace allo
