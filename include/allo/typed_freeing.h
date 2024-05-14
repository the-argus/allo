#pragma once
#include "allo/detail/abstracts.h"
#include "allo/status.h"

#ifndef ALLO_DISABLE_TYPEINFO
#ifndef ALLO_USE_RTTI
#include "ctti/typename.h"
#endif
#endif

namespace allo {
template <typename T, typename Freer>
inline allocation_status_t free_one(Freer& allocator, T& item) noexcept
{
    static_assert(!std::is_reference_v<T>, "Can't free a reference type");
    static_assert(
        detail::is_freer<Freer>,
        "The type given as the freer cannot be used to free the item.");
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
        zl::raw_slice(*reinterpret_cast<uint8_t*>(std::addressof(item)),
                      sizeof(T)),
        typehash);
}

template <typename T, typename Freer>
inline allocation_status_t free(Freer& allocator,
                                const zl::slice<T> items) noexcept
{
    static_assert(
        detail::is_freer<Freer>,
        "The type given as the freer cannot be used to free the item.");
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
        zl::raw_slice(*reinterpret_cast<uint8_t*>(items.data()),
                      sizeof(T) * items.size()),
        typehash);
}

#define ALLO_ALLOW_DESTRUCTORS

#ifdef ALLO_ALLOW_DESTRUCTORS
template <typename T, typename Freer>
inline allocation_status_t destroy_one(Freer& allocator, T& item) noexcept
{
    static_assert(
        detail::is_freer<Freer>,
        "The type given as the freer cannot be used to free the item.");
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
        *reinterpret_cast<uint8_t*>(std::addressof(item)), sizeof(T));
    allocation_status_t status = allocator.free_status(bytes, typehash);
    if (!status.okay())
        return status;
    item.~T();
    status = allocator.free_bytes(bytes, typehash);
    // BUG: EVIL:
    // horrible error possible if multithreading. need way to get ownership
    // of allocator
    assert(status.okay());
    return status;
}

template <typename T, typename Freer>
allocation_status_t destroy_many(Freer& allocator,
                                 const zl::slice<T> items) noexcept
{
    static_assert(
        detail::is_freer<Freer>,
        "The type given as the freer cannot be used to free the item.");
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
    auto bytes = zl::raw_slice(*reinterpret_cast<uint8_t*>(items.data()),
                               sizeof(T) * items.size());
    allocation_status_t status = allocator.free_status(bytes, typehash);
    if (!status.okay())
        return status;
    for (auto& item : items) {
        item.~T();
    }
    status = allocator.free_bytes(bytes, typehash);
    // BUG: EVIL:
    // horrible error possible if multithreading. need way to get ownership
    // of allocator
    assert(status.okay());
    return status;
}
#endif
#undef ALLO_FREE_VALIDITY_CHECKS

} // namespace allo
