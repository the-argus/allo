#pragma once
#include "allo/allocator_interfaces.h"

namespace allo {
/// Allocate memory for one "T". The contents of this memory is undefined.
template <typename T, uint8_t alignment = alignof(T)>
inline zl::res<T &, AllocationStatusCode>
alloc_one(allocator_t &allocator) noexcept
{
    static_assert(
        alignment >= alignof(T),
        "Alignment less than the type being allocated is probably not what "
        "you wanted. Use alloc_bytes if you have to.");
    auto res = allocator.alloc_bytes(sizeof(T), alignment, 0);
    if (!res.okay())
        return res.err();
    const auto &mem = res.release_ref();
    assert(sizeof(T) == mem.size());
    return *reinterpret_cast<T *>(mem.data());
}

/// Allocate memory for a contiguous buffer of a number of items of type T.
/// The contents of this memory is undefined.
template <typename T, uint8_t alignment = alignof(T)>
inline zl::res<zl::slice<T>, AllocationStatusCode> alloc(allocator_t &allocator,
                                                         size_t number) noexcept
{
    static_assert(
        alignment >= alignof(T),
        "Alignment less than the type being allocated is probably not what "
        "you wanted. Use alloc_bytes if you have to.");
    auto res = allocator.alloc_bytes(sizeof(T) * number, alignment, 0);
    if (!res.okay())
        return res.err();
    const auto &mem = res.release_ref();
    assert(sizeof(T) * number == mem.size());
    return zl::raw_slice(reinterpret_cast<T *>(mem.data()), number);
}

/// Create one item of type T using an allocator, constructing it with "args".
/// Effectively identical to the "new" keyword.
///
/// If a constructor throws an exception, the function will exit but the memory
/// will not be freed, and leak.
template <typename T, typename... Args>
inline zl::res<T &, AllocationStatusCode> construct_one(allocator_t &allocator,
                                                        Args &&...args)
{
    static_assert(std::is_constructible_v<T, Args...>,
                  "Type is not constructible with those arguments.");
    auto mem = alloc_one<T>(allocator);
    if (!mem.okay())
        return mem.err();
    auto &item = mem.release();
    new (std::addressof(item)) T(std::forward<Args>(args)...);
    return item;
}

/// Allocate a contiguous batch of memory containing "number" items of type T,
/// and construct every one of them with the args.
///
/// If a constructor throws an exception, the function will exit but the memory
/// will not be freed, and leak.
template <typename T, typename... Args>
inline zl::res<zl::slice<T>, AllocationStatusCode>
construct_many(allocator_t &allocator, size_t number, Args &&...args)
{
    static_assert(std::is_constructible_v<T, Args...>,
                  "Type is not constructible with those arguments.");
    auto mem = alloc<T>(allocator, number);
    if (!mem.okay())
        return mem.err();
    const auto &slice = mem.release_ref();

    for (T &item : slice) {
        new (std::addressof(item)) T(std::forward<Args>(args)...);
    }

    return slice;
}
} // namespace allo
