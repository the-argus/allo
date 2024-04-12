#pragma once

#include <cstddef>
namespace allo::detail {
template <typename T> inline constexpr size_t calculate_segment_size() noexcept
{
    // we could do prime factoring here... but its compile time and guess-and
    // check is easier
    size_t size = 64;
    while (size < sizeof(T) || size % sizeof(T) != 0) {
        size += 64;
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
};
} // namespace allo::detail
