#pragma once
#include "allo/allocator_interfaces.h"

#ifndef ALLO_DISABLE_TYPEINFO
#ifndef ALLO_USE_RTTI
#include "ctti/typename.h"
#endif
#endif

namespace allo {
template <typename T, typename Freer>
inline allocation_status_t free_one(Freer &allocator, T &item) noexcept
{
    static_assert(std::is_base_of_v<detail::stack_freer_t, Freer>,
                  "Can't use the given type to perform a free");
    constexpr bool is_valid_interface =
        std::is_base_of_v<detail::allocator_interface_t, Freer>;
    constexpr bool is_valid_allocator =
        std::is_base_of_v<detail::dynamic_allocator_base_t, Freer>;
    static_assert((is_valid_interface &&
                   std::is_base_of_v<detail::i_stack_free, Freer>) ||
                      (is_valid_allocator &&
                       std::is_base_of_v<detail::stack_freer_t, Freer>),
                  "Can't use the given type to perform a free");
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
    if constexpr (is_valid_allocator) {
        return allocator.free_bytes(
            zl::raw_slice(*reinterpret_cast<uint8_t *>(std::addressof(item)),
                          sizeof(T)),
            typehash);
    } else {
        return detail::i_stack_free::_free_bytes(
            std::addressof(allocator),
            zl::raw_slice(*reinterpret_cast<uint8_t *>(std::addressof(item)),
                          sizeof(T)),
            typehash);
    }
}

template <typename T, typename Freer>
inline allocation_status_t free(Freer &allocator, const zl::slice<T> items) noexcept
{
    static_assert(std::is_base_of_v<detail::stack_freer_t, Freer>,
                  "Can't use the given type to perform a free");
    constexpr bool is_valid_interface =
        std::is_base_of_v<detail::allocator_interface_t, Freer>;
    constexpr bool is_valid_allocator =
        std::is_base_of_v<detail::dynamic_allocator_base_t, Freer>;
    static_assert((is_valid_interface &&
                   std::is_base_of_v<detail::i_stack_free, Freer>) ||
                      (is_valid_allocator &&
                       std::is_base_of_v<detail::stack_freer_t, Freer>),
                  "Can't use the given type to perform a free");
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
    if constexpr (is_valid_allocator) {
        return allocator.free_bytes(
            zl::raw_slice(*reinterpret_cast<uint8_t *>(items.data()),
                          sizeof(T) * items.size()),
            typehash);
    } else {
        return detail::i_stack_free::_free_bytes(
            std::addressof(allocator),
            zl::raw_slice(*reinterpret_cast<uint8_t *>(items.data()),
                          sizeof(T) * items.size()),
            typehash);
    }
}

template <typename T, typename Freer>
inline allocation_status_t destroy_one(Freer &allocator, T &item) noexcept
{
    constexpr bool is_valid_interface =
        std::is_base_of_v<detail::allocator_interface_t, Freer>;
    constexpr bool is_valid_allocator =
        std::is_base_of_v<detail::dynamic_allocator_base_t, Freer>;
    static_assert((is_valid_interface &&
                   std::is_base_of_v<detail::i_stack_free, Freer>) ||
                      (is_valid_allocator &&
                       std::is_base_of_v<detail::stack_freer_t, Freer>),
                  "Can't use the given type to perform a free");
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
    allocation_status_t status = AllocationStatusCode::Okay;
    if constexpr (is_valid_allocator) {
        status = allocator.free_status(bytes, typehash);
    } else {
        status = detail::i_stack_free::_free_status(std::addressof(allocator),
                                                    bytes, typehash);
    }
    if (!status.okay())
        return status;
    item.~T();
    if constexpr (is_valid_allocator) {
        status = allocator.free_bytes(bytes, typehash);
    } else {
        status = detail::i_stack_free::_free_bytes(std::addressof(allocator),
                                                   bytes, typehash);
    }
    if (!status.okay()) {
        // BUG: EVIL:
        // horrible error possible if multithreading. need way to get ownership
        // of allocator
        std::abort();
    }
    return status;
}

template <typename T, typename Freer>
allocation_status_t
destroy_many(std::enable_if_t<std::is_base_of_v<detail::stack_freer_t, Freer> &&
                                  !std::is_reference_v<T>,
                              Freer> &allocator,
             const zl::slice<T> items) noexcept
{
    constexpr bool is_valid_interface =
        std::is_base_of_v<detail::allocator_interface_t, Freer>;
    constexpr bool is_valid_allocator =
        std::is_base_of_v<detail::dynamic_allocator_base_t, Freer>;
    static_assert((is_valid_interface &&
                   std::is_base_of_v<detail::i_stack_free, Freer>) ||
                      (is_valid_allocator &&
                       std::is_base_of_v<detail::stack_freer_t, Freer>),
                  "Can't use the given type to perform a free");
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
    allocation_status_t status = AllocationStatusCode::Okay;
    if constexpr (is_valid_allocator) {
        status = allocator.free_status(bytes, typehash);
    } else {
        status = detail::i_stack_free::_free_status(std::addressof(allocator),
                                                    bytes, typehash);
    }
    if (!status.okay())
        return status;
    for (auto &item : items) {
        item.~T();
    }
    if constexpr (is_valid_allocator) {
        status = allocator.free_bytes(bytes, typehash);
    } else {
        status = detail::i_stack_free::_free_bytes(std::addressof(allocator),
                                                   bytes, typehash);
    }
    if (!status.okay()) {
        // BUG: EVIL:
        // horrible error possible if multithreading. need way to get ownership
        // of allocator
        std::abort();
    }
    return status;
}

} // namespace allo
