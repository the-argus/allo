#pragma once
#include "allo/allocator_interfaces.h"

#ifndef ALLO_DISABLE_TYPEINFO
#ifndef ALLO_USE_RTTI
#include "ctti/typename.h"
#endif
#endif

namespace allo {
template <typename T, typename Freer>
allocation_status_t
free_one(std::enable_if_t<std::is_base_of_v<detail::stack_freer_t, Freer>,
                          Freer> &allocator,
         T &item) noexcept
{
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
allocation_status_t
free(std::enable_if_t<std::is_base_of_v<detail::stack_freer_t, Freer>, Freer>
         &allocator,
     const zl::slice<T> items) noexcept
{
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
allocation_status_t
destroy_one(std::enable_if_t<std::is_base_of_v<detail::stack_freer_t, Freer>,
                             Freer> &allocator,
            T &item) noexcept
{
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
    auto status = allocator.free_status(bytes, typehash);
    if (!status.okay())
        return status;
    item.~T();
    auto actual_status = allocator.free_bytes(bytes, typehash);
    if (!actual_status.okay()) {
        // horrible error possible if multithreading. need way to get ownership
        // of allocator
        std::abort();
    }
    return actual_status;
}

template <typename T, typename Freer>
allocation_status_t
destroy_many(std::enable_if_t<std::is_base_of_v<detail::stack_freer_t, Freer>,
                              Freer> &allocator,
             const zl::slice<T> items) noexcept
{
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
    auto status = allocator.free_status(bytes, typehash);
    if (!status.okay())
        return status;
    for (auto &item : items) {
        item.~T();
    }
    auto actual_status = allocator.free_bytes(bytes, typehash);
    if (!actual_status.okay()) {
        // horrible error possible if multithreading. need way to get ownership
        // of allocator
        std::abort();
    }
    return actual_status;
}

} // namespace allo
