#pragma once

#include <cstddef>
#include "allo/detail/cache_line_size.h"
namespace allo::detail {
/// Get how many items of type T need to be put next to each other to make a
/// block divisible by cache_line_size
template <typename T> inline constexpr size_t calculate_segment_size() noexcept
{
    // we could do prime factoring here... but its compile time and guess-and
    // check is easier
    size_t size = cache_line_size;
    while (size < sizeof(T) || size % sizeof(T) != 0) {
        size += cache_line_size;
    }
    return size;
}

/// Same as calculate_segment_size, except if we have a single item of type
/// Endcap at the end of each cache-line-divisible block
template <typename T, typename Endcap>
inline constexpr size_t calculate_segment_size_with_endcap() noexcept
{
    // we could do prime factoring here... but its compile time and guess-and
    // check is easier
    size_t size = cache_line_size;
    while (size < sizeof(T) + sizeof(Endcap) ||
           (size - sizeof(Endcap)) % sizeof(T) != 0) {
        size += cache_line_size;
    }
    return size;
}

/// Wrapper around calculate_segment_size to force it to be compile time, since
/// otherwise it could be expensive
template <typename T> struct segment_size
{
    static constexpr size_t value = calculate_segment_size<T>();
    static_assert(value < (64UL * 20UL),
                  "Type given to segmented data structure is either too big or "
                  "oddly sized so that each segment cannot be only a few cache "
                  "lines long. This is an edge case which has not been tested. "
                  "Report this as a bug.");
    static_assert(value % cache_line_size == 0,
                  "calculate_segment_size_with_endcap yielded result which "
                  "does not evenly fit in cache lines");
};

template <typename T, typename Endcap> struct segment_size_with_endcap
{
    static constexpr size_t value =
        calculate_segment_size_with_endcap<T, Endcap>();
    static constexpr size_t number_of_items =
        (value - sizeof(Endcap)) / sizeof(T);
    static_assert(value < (64UL * 20UL),
                  "Type given to segmented data structure is either too big or "
                  "oddly sized so that each segment cannot be only a few cache "
                  "lines long. This is an edge case which has not been tested. "
                  "Report this as a bug.");
    static_assert(value % cache_line_size == 0,
                  "calculate_segment_size_with_endcap yielded result which "
                  "does not evenly fit in cache lines");
    static_assert((value - sizeof(Endcap)) % sizeof(T) == 0,
                  "calculate_segment_size_with_endcap yielded a result which "
                  "does not evenly fit a number of items of type T.");
};
} // namespace allo::detail
