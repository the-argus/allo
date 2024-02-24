#pragma once
#include "allo/allocator_interfaces.h"

#ifndef ALLO_DISABLE_TYPEINFO
#ifndef ALLO_USE_RTTI
#include "ctti/typename.h"
#endif
#endif

namespace allo {
/// Allocate memory for one "T". The contents of this memory is undefined.
/// T must not be a reference type.
template <typename T, typename Allocator, uint8_t alignment = alignof(T)>
inline zl::res<T &, AllocationStatusCode>
alloc_one(Allocator &allocator) noexcept
{
    constexpr bool is_valid_interface =
        std::is_base_of_v<detail::allocator_interface_t, Allocator>;
    constexpr bool is_valid_allocator =
        std::is_base_of_v<detail::dynamic_allocator_base_t, Allocator>;
    static_assert(
        (is_valid_interface && std::is_base_of_v<detail::i_alloc, Allocator>) ||
            (is_valid_allocator &&
             std::is_base_of_v<detail::allocator_t, Allocator>),
        "Cannot use given type to perform allocations");
    static_assert(!std::is_reference_v<T>, "Cannot allocate a reference type.");
    static_assert(
        alignment >= alignof(T),
        "Not allocating at lower alignment than that of the given type.");
    // very large alignment (2^64) is an error pretty much always
    static_assert(detail::alignment_exponent(alignment) != 64,
                  "Invalid alignment provided. Make sure its a power of 2.");
    size_t typehash =
#ifndef ALLO_DISABLE_TYPEINFO
#ifdef ALLO_USE_RTTI
        typeid(T).hash_code();
#else
        ctti::nameof<T>().hash();
#endif
#else
        0;
#endif
    zl::opt<allocation_result_t> result;
    if constexpr (is_valid_allocator) {
        result = allocator.alloc_bytes(
            sizeof(T), detail::alignment_exponent(alignment), typehash);
    } else {
        result = IAlloc::_alloc_bytes(std::addressof(allocator), sizeof(T),
                                      detail::alignment_exponent(alignment),
                                      typehash);
    }
    allocation_result_t &actual = result.value();
    if (!actual.okay())
        return actual.err();
    const auto &mem = actual.release_ref();
    assert(sizeof(T) == mem.size());
    return *reinterpret_cast<T *>(mem.data());
}

/// Allocate memory for a contiguous buffer of a number of items of type T.
/// The contents of this memory is undefined. T must not be a reference type.
template <typename T, typename Allocator, uint8_t alignment = alignof(T)>
inline zl::res<zl::slice<T>, AllocationStatusCode> alloc(Allocator &allocator,
                                                         size_t number) noexcept
{
    constexpr bool is_valid_interface =
        std::is_base_of_v<detail::allocator_interface_t, Allocator>;
    constexpr bool is_valid_allocator =
        std::is_base_of_v<detail::dynamic_allocator_base_t, Allocator>;
    static_assert(
        (is_valid_interface && std::is_base_of_v<detail::i_alloc, Allocator>) ||
            (is_valid_allocator &&
             std::is_base_of_v<detail::allocator_t, Allocator>),
        "Cannot use given type to perform allocations");
    static_assert(!std::is_reference_v<T>, "Cannot allocate a reference type.");
    static_assert(
        alignment >= alignof(T),
        "Not allocating at lower alignment than that of the given type.");
    // very large alignment (2^64) is an error pretty much always
    static_assert(detail::alignment_exponent(alignment) != 64,
                  "Invalid alignment provided. Make sure its a power of 2.");
    size_t typehash =
#ifndef ALLO_DISABLE_TYPEINFO
#ifdef ALLO_USE_RTTI
        typeid(T).hash_code();
#else
        ctti::nameof<T>().hash();
#endif
#else
        0;
#endif
    zl::opt<allocation_result_t> mres;
    if constexpr (is_valid_allocator) {
        mres = allocator.alloc_bytes(sizeof(T) * number,
                                     detail::alignment_exponent(alignment),
                                     typehash);
    } else {
        mres = IAlloc::_alloc_bytes(
            std::addressof(allocator), sizeof(T) * number,
            detail::alignment_exponent(alignment), typehash);
    }
    auto &res = mres.value();
    if (!res.okay())
        return res.err();
    const auto &mem = res.release_ref();
    assert(sizeof(T) * number == mem.size());
    return zl::raw_slice(*reinterpret_cast<T *>(mem.data()), number);
}

/// Create one item of type T using an allocator, constructing it with "args".
/// Effectively identical to the "new" keyword. T must not be a reference type.
///
/// If a constructor throws an exception, the function will exit but the memory
/// will not be freed, and leak.
template <typename T, typename Allocator, typename... Args>
inline zl::res<T &, AllocationStatusCode> construct_one(Allocator &allocator,
                                                        Args &&...args)
{
    constexpr bool is_valid_interface =
        std::is_base_of_v<detail::allocator_interface_t, Allocator>;
    constexpr bool is_valid_allocator =
        std::is_base_of_v<detail::dynamic_allocator_base_t, Allocator>;
    static_assert(
        (is_valid_interface && std::is_base_of_v<detail::i_alloc, Allocator>) ||
            (is_valid_allocator &&
             std::is_base_of_v<detail::allocator_t, Allocator>),
        "Cannot use given type to perform allocations");
    static_assert(!std::is_reference_v<T>, "Cannot allocate a reference type.");
    static_assert(std::is_constructible_v<T, Args...>,
                  "Type is not constructible with those arguments.");
    auto mem = alloc_one<T, Allocator>(allocator);
    if (!mem.okay())
        return mem.err();
    auto &item = mem.release();
    new (std::addressof(item)) T(std::forward<Args>(args)...);
    return item;
}

/// Allocate a contiguous batch of memory containing "number" items of type T,
/// and construct every one of them with the args. T must not be a reference
/// type
///
/// If a constructor throws an exception, the function will exit but the memory
/// will not be freed, and leak.
template <typename T, typename Allocator, typename... Args>
inline zl::res<zl::slice<T>, AllocationStatusCode>
construct_many(Allocator &allocator, size_t number, Args &&...args)
{
    constexpr bool is_valid_interface =
        std::is_base_of_v<detail::allocator_interface_t, Allocator>;
    constexpr bool is_valid_allocator =
        std::is_base_of_v<detail::dynamic_allocator_base_t, Allocator>;
    static_assert(
        (is_valid_interface && std::is_base_of_v<detail::i_alloc, Allocator>) ||
            (is_valid_allocator &&
             std::is_base_of_v<detail::allocator_t, Allocator>),
        "Cannot use given type to perform allocations");
    static_assert(!std::is_reference_v<T>, "Cannot allocate a reference type.");
    static_assert(std::is_constructible_v<T, Args...>,
                  "Type is not constructible with those arguments.");
    auto mem = alloc<T, Allocator>(allocator, number);
    if (!mem.okay())
        return mem.err();
    const auto &slice = mem.release_ref();

    for (T &item : slice) {
        new (std::addressof(item)) T(std::forward<Args>(args)...);
    }

    return slice;
}
} // namespace allo
