#pragma once
#include "allo/detail/abstracts.h"

#ifndef ALLO_DISABLE_TYPEINFO
#ifndef ALLO_USE_RTTI
#include "ctti/typename.h"
#endif
#endif

namespace allo {
/// Allocate memory for one "T". The contents of this memory is undefined.
/// T must not be a reference type.
template <typename T, typename Allocator, uint8_t alignment = alignof(T)>
[[nodiscard]] inline zl::res<T &, AllocationStatusCode>
alloc_one(Allocator &allocator) noexcept
{
    constexpr bool is_valid_interface =
        std::is_base_of_v<detail::allocator_common_t, Allocator>;
    constexpr bool is_valid_allocator =
        std::is_base_of_v<detail::dynamic_allocator_base_t, Allocator>;
#ifndef ALLO_ALLOW_NONTRIVIAL_COPY
    static_assert(std::is_trivially_copyable_v<T>,
                  "Refusing to allocate non-trivially copyable type which "
                  "could cause UB on reallocation.");
#endif
    static_assert(is_valid_interface || is_valid_allocator,
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
    if constexpr (std::is_same_v<T, uint8_t>) {
        typehash = 0;
    }
    allocation_result_t result = allocator.alloc_bytes(
        sizeof(T), detail::alignment_exponent(alignment), typehash);
    if (!result.okay())
        return result.err();
    const auto &mem = result.release_ref();
    assert(sizeof(T) == mem.size());
    return *reinterpret_cast<T *>(mem.data());
}

/// Allocate memory for a contiguous buffer of a number of items of type T.
/// The contents of this memory is undefined. T must not be a reference type.
template <typename T, typename Allocator, uint8_t alignment = alignof(T)>
[[nodiscard]] inline zl::res<zl::slice<T>, AllocationStatusCode>
alloc(Allocator &allocator, size_t number) noexcept
{
    constexpr bool is_valid_interface =
        std::is_base_of_v<detail::allocator_common_t, Allocator>;
    constexpr bool is_valid_allocator =
        std::is_base_of_v<detail::dynamic_allocator_base_t, Allocator>;
#ifndef ALLO_ALLOW_NONTRIVIAL_COPY
    static_assert(std::is_trivially_copyable_v<T>,
                  "Refusing to allocate non-trivially copyable type which "
                  "could cause UB on reallocation.");
#endif
    static_assert(is_valid_interface || is_valid_allocator,
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
    if constexpr (std::is_same_v<T, uint8_t>) {
        typehash = 0;
    }
    allocation_result_t res = allocator.alloc_bytes(
        sizeof(T) * number, detail::alignment_exponent(alignment), typehash);
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
[[nodiscard]] inline zl::res<T &, AllocationStatusCode>
construct_one(Allocator &allocator, Args &&...args)
{
#ifndef ALLO_ALLOW_DESTRUCTORS
    static_assert(std::is_trivially_destructible_v<T>,
                  "Refusing to construct_one for a type with a non-trivial "
                  "destructor which will never be called");
#endif
    constexpr bool is_valid_interface =
        std::is_base_of_v<detail::allocator_common_t, Allocator>;
    constexpr bool is_valid_allocator =
        std::is_base_of_v<detail::dynamic_allocator_base_t, Allocator>;
    static_assert(is_valid_interface || is_valid_allocator,
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
[[nodiscard]] inline zl::res<zl::slice<T>, AllocationStatusCode>
construct_many(Allocator &allocator, size_t number, Args &&...args)
{
#ifndef ALLO_ALLOW_DESTRUCTORS
    static_assert(std::is_trivially_destructible_v<T>,
                  "Refusing to construct_one for a type with a non-trivial "
                  "destructor which will never be called");
#endif
    constexpr bool is_valid_interface =
        std::is_base_of_v<detail::allocator_common_t, Allocator>;
    constexpr bool is_valid_allocator =
        std::is_base_of_v<detail::dynamic_allocator_base_t, Allocator>;
    static_assert(is_valid_interface || is_valid_allocator,
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
