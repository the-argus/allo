#pragma once
#include "allo/abstracts.h"

#ifndef ALLO_DISABLE_TYPEINFO
#ifndef ALLO_USE_RTTI
#include "ctti/typename.h"
#endif
#endif

namespace allo {
#define ALLO_FREE_VALIDITY_CHECKS                                    \
    constexpr bool is_valid_interface =                              \
        std::is_base_of_v<detail::dynamic_stack_allocator_t, Freer>; \
    constexpr bool is_valid_allocator =                              \
        std::is_base_of_v<detail::dynamic_allocator_base_t, Freer>;  \
    static_assert(!std::is_same_v<Freer, scratch_allocator_t>,       \
                  "Cannot free with a scratch allocator.");          \
    static_assert(is_valid_interface || is_valid_allocator,          \
                  "Cannot use given type to perform a free");

template <typename T, typename Freer>
inline allocation_status_t free_one(Freer &allocator, T &item) noexcept
{
    ALLO_FREE_VALIDITY_CHECKS
    static_assert(!std::is_reference_v<T>, "Can't free a reference type");
    const size_t typehash =
#ifndef ALLO_DISABLE_TYPEINFO
#ifdef ALLO_USE_RTTI
        typeid(T).hash_code();
#else
        ctti::nameof<T>().hash();
#endif
#else
        0;
#endif
    return allocator.free_bytes(
        zl::raw_slice(*reinterpret_cast<uint8_t *>(std::addressof(item)),
                      sizeof(T)),
        typehash);
}

template <typename T, typename Freer>
inline allocation_status_t free(Freer &allocator,
                                const zl::slice<T> items) noexcept
{
    ALLO_FREE_VALIDITY_CHECKS
    static_assert(!std::is_reference_v<T>, "Can't free a reference type");
    const size_t typehash =
#ifndef ALLO_DISABLE_TYPEINFO
#ifdef ALLO_USE_RTTI
        typeid(T).hash_code();
#else
        ctti::nameof<T>().hash();
#endif
#else
        0;
#endif
    return allocator.free_bytes(
        zl::raw_slice(*reinterpret_cast<uint8_t *>(items.data()),
                      sizeof(T) * items.size()),
        typehash);
}

template <typename T, typename Freer>
inline allocation_status_t destroy_one(Freer &allocator, T &item) noexcept
{
    ALLO_FREE_VALIDITY_CHECKS
    static_assert(!std::is_reference_v<T>, "Can't free a reference type");
    const size_t typehash =
#ifndef ALLO_DISABLE_TYPEINFO
#ifdef ALLO_USE_RTTI
        typeid(T).hash_code();
#else
        ctti::nameof<T>().hash();
#endif
#else
        0;
#endif
    auto bytes = zl::raw_slice(
        *reinterpret_cast<uint8_t *>(std::addressof(item)), sizeof(T));
    allocation_status_t status = allocator.free_status(bytes, typehash);
    if (!status.okay())
        return status;
    item.~T();
    status = allocator.free_bytes(bytes, typehash);
    if (!status.okay()) {
        // BUG: EVIL:
        // horrible error possible if multithreading. need way to get ownership
        // of allocator
        std::abort();
    }
    return status;
}

template <typename T, typename Freer>
allocation_status_t destroy_many(Freer &allocator,
                                 const zl::slice<T> items) noexcept
{
    ALLO_FREE_VALIDITY_CHECKS
    static_assert(!std::is_reference_v<T>, "Can't free a reference type");
    const size_t typehash =
#ifndef ALLO_DISABLE_TYPEINFO
#ifdef ALLO_USE_RTTI
        typeid(T).hash_code();
#else
        ctti::nameof<T>().hash();
#endif
#else
        0;
#endif
    auto bytes = zl::raw_slice(*reinterpret_cast<uint8_t *>(items.data()),
                               sizeof(T) * items.size());
    allocation_status_t status = allocator.free_status(bytes, typehash);
    if (!status.okay())
        return status;
    for (auto &item : items) {
        item.~T();
    }
    status = allocator.free_bytes(bytes, typehash);
    if (!status.okay()) {
        // BUG: EVIL:
        // horrible error possible if multithreading. need way to get ownership
        // of allocator
        std::abort();
    }
    return status;
}
#undef ALLO_FREE_VALIDITY_CHECKS

} // namespace allo
