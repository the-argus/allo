#pragma once
// c allocator is needed because it breaks abstraction so we need access to its
// impl details
#include "allo/c_allocator.h"
#include "allo/detail/abstracts.h"
#include "allo/detail/alignment.h"
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
    // static assert to help catch the abstraction-breaking problem with
    // c_allocator_t
    static_assert(!std::is_same_v<Allocator, c_allocator_t>,
                  "The c allocator does not provide remap functionality, you "
                  "must use allo::realloc.");
    static_assert(detail::is_reallocator<Allocator>,
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

    bytes_t &newmem = remapped.release_ref();
    assert(newmem.data() == (uint8_t *)original.data());

    return zl::raw_slice(*original.data(), new_size);
}

/// Remap, or if remap fails, create an entirely new, differently sized
/// allocation and copy the contents of the first allocation to that one.
template <typename Allocator>
inline zl::res<bytes_t, AllocationStatusCode>
realloc_bytes(Allocator &allocator, bytes_t original, size_t old_typehash,
              size_t new_size, uint8_t new_alignment_exponent,
              size_t typehash) noexcept
{
    static_assert(detail::is_reallocator<Allocator>,
                  "Cannot use given type to perform reallocations");

    // threadsafe heap allocators are already required to supply an atomic
    // realloc operation, so just use that if its available
    if constexpr (std::is_base_of_v<
                      detail::abstract_threadsafe_heap_allocator_t,
                      Allocator>) {
        return allocator.threadsafe_realloc_bytes(original, old_typehash,
                                                  new_size, typehash);
    } else {
        // allocators that aren't threadsafe heap allocators:
        auto remap =
            allocator.remap_bytes(original, old_typehash, new_size, typehash);
        if (remap.okay())
            return remap;

        static_assert(sizeof(original.data()) == sizeof(size_t));
        auto new_allocation =
            allocator.alloc_bytes(new_size, new_alignment_exponent, typehash);
        if (new_allocation.okay()) {
            const bool enlarging = original.size() < new_size;
            const bytes_t source =
                enlarging ? original : bytes_t(original, 0, new_size);
            bytes_t dest = enlarging ? bytes_t(new_allocation.release_ref(), 0,
                                               original.size())
                                     : new_allocation.release_ref();
            const bool status = zl::memcopy(dest, source);
            assert(status);
            allocator.free_bytes(original, old_typehash);
            // TODO: assert or something here maybe? should we allow failed
            // frees? probably a warning log message would be good
            return dest;
        }
        return new_allocation.err();
    }
}

// Simple wrapper around realloc_bytes for trivially copyable types
template <typename T, typename Allocator, size_t alignment = alignof(T)>
inline zl::res<zl::slice<T>, AllocationStatusCode>
realloc(Allocator &allocator, zl::slice<T> original, size_t new_size) noexcept
{
    static_assert(detail::is_reallocator<Allocator>,
                  "Cannot use given type to perform reallocations");

    bytes_t original_bytes =
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

    constexpr uint8_t aex = detail::nearest_alignment_exponent(alignment);

    auto realloc_res = realloc_bytes(allocator, original_bytes, typehash,
                                     new_size_bytes, aex, typehash);
    if (!realloc_res.okay())
        return realloc_res.err();

    return zl::raw_slice(*reinterpret_cast<T *>(realloc_res.release().data()),
                         new_size);
}

/// When calling realloc with T = uint8_t, it just calls realloc_bytes.
template <typename Allocator, size_t alignment = 32>
inline zl::res<bytes_t, AllocationStatusCode>
realloc(Allocator &allocator, bytes_t original, size_t new_size) noexcept
{
    constexpr uint8_t aex = detail::nearest_alignment_exponent(alignment);
    return realloc_bytes(allocator, original, 0, new_size, aex, 0);
}
} // namespace allo
