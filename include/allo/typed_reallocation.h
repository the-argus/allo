#pragma once
// c allocator is needed because it breaks abstraction so we need access to its
// impl details
#include "allo/detail/abstracts.h"
#include "allo/detail/alignment.h"
#include "allo/detail/is_threadsafe.h"
#include "allo/detail/is_threadsafe_runtime.h"
#include <ziglike/stdmem.h>

#ifndef ALLO_DISABLE_TYPEINFO
#ifndef ALLO_USE_RTTI
#include "ctti/typename.h"
#endif
#endif

namespace allo {
class c_allocator_t;
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
    assert(allocator.type() != detail::AllocatorType::CAllocator);

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
template <typename T, typename Allocator, size_t alignment = alignof(T)>
inline zl::res<zl::slice<T>, AllocationStatusCode>
realloc(Allocator &allocator, zl::slice<T> original, size_t new_size) noexcept
{
    static_assert(detail::is_reallocator<Allocator>,
                  "Cannot use given type to perform reallocations");
    if (original.size() == new_size) [[unlikely]]
        return original;

#if defined(ALLO_DISABLE_TYPEINFO)
    const size_t typehash = 0;
#elif defined(ALLO_USE_RTTI)
    const size_t typehash = typeid(T).hash_code();
#else
    constexpr size_t typehash = ctti::nameof<T>().hash();
#endif

    const bytes_t original_bytes =
        zl::raw_slice(*reinterpret_cast<uint8_t *>(original.data()),
                      original.size() * sizeof(T));
    const size_t new_size_bytes = new_size * sizeof(T);

    // first just try to remap
    auto remap = allocator.remap_bytes(original_bytes, typehash, new_size_bytes,
                                       typehash);
    if (remap.okay()) {
        return zl::raw_slice(*reinterpret_cast<T *>(remap.release().data()),
                             new_size);
    }

    if constexpr (detail::is_threadsafe<Allocator>) {
        // threadsafe heap allocators are already required to supply an atomic
        // realloc operation, so just use that if its known available at
        // compile time
        return allocator.threadsafe_realloc_bytes(original, typehash, new_size,
                                                  typehash);
    } else {
        // if we can determine that this allocator is threadsafe, then downcast
        // it and call its threadsafe reallocation function.
        if (detail::is_threadsafe_runtime(allocator)) {
            auto stack_to_threadsafe_downcast =
                [original_bytes, new_size, new_size_bytes,
                 typehash](detail::abstract_stack_allocator_t &ally)
                -> zl::res<zl::slice<T>, AllocationStatusCode> {
                detail::abstract_threadsafe_heap_allocator_t &ts =
                    *reinterpret_cast<
                        detail::abstract_threadsafe_heap_allocator_t *>(&ally);
                auto res = ts.threadsafe_realloc_bytes(
                    original_bytes, typehash, new_size_bytes, typehash);
                if (!res.okay())
                    return res.err();
                return zl::raw_slice(
                    *reinterpret_cast<T *>(res.release().data()), new_size);
            };
            return stack_to_threadsafe_downcast(allocator);
        }

        constexpr uint8_t aex = detail::nearest_alignment_exponent(alignment);

        auto new_allocation =
            allocator.alloc_bytes(new_size_bytes, aex, typehash);
        if (!new_allocation.okay())
            return new_allocation.err();

        const bool enlarging = original.size() < new_size;
        const zl::slice<T> source =
            enlarging ? original : zl::slice<T>(original, 0, new_size);
        zl::slice<T> dest = zl::raw_slice(
            *reinterpret_cast<T *>(new_allocation.release_ref().data()),
            enlarging ? original.size() : new_size);
        // if we're trivially copyable, use memcpy
        if constexpr (std::is_trivially_copyable_v<T>) {
            const bool status = zl::memcopy(dest, source);
            assert(status);
        } else {
            for (size_t i = 0; i < source.size(); ++i) {
                static_assert(std::is_move_constructible_v<T>,
                              "Cannot call realloc on slice of type which is "
                              "not movable nor trivially copyable");
                new (dest.data() + i) T(std::move(source.data()[i]));
            }
        }

        if constexpr (!std::is_trivially_destructible_v<T>) {
            // some have been chopped off, call their destructors
            if (!enlarging) {
                const zl::slice<T> removed(original, new_size, original.size());
                for (T &item : removed) {
                    item.~T();
                }
            }
        }

        allocator.free_bytes(original_bytes, typehash);
        // TODO: assert or something here maybe? should we allow failed
        // frees? probably a warning log message would be good
        return dest;
    }
}
} // namespace allo
