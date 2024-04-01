#pragma once
#include <cstddef>
#include <cstdint>

namespace allo::detail {
/// Take a given number divisible by two and find what n is in 2^n = number.
/// Returns 64 (ie. 2^64) as an error value
inline constexpr uint8_t alignment_exponent(size_t alignment)
{
    constexpr auto bits = sizeof(size_t) * 8;
    for (size_t i = 0; i < bits; ++i) {
        if (((size_t(1) << i) & alignment) == alignment) {
            return i;
        }
    }
    return bits;
}

/// Take a memory address as a unsigned long and return the nearest power of 2
/// that it is divisible by. Useful for figuring out what the alignment is of
/// all items in an array of things of a given size (ie in the block allocator)
///
/// If you pass in a pointer cast to a size_t, then the nearest alignment
/// exponent will be as big as the alignment exponent of the allocation that
/// originally created that pointer, *or bigger*.
inline constexpr uint8_t nearest_alignment_exponent(size_t num)
{
    constexpr auto bits = sizeof(size_t) * 8;
    size_t mask = 1;
    for (size_t i = 0; i < bits; ++i) {
        if ((mask & num) != 0) {
            return i;
        }
        mask = mask << 1;
        mask += 1;
    }
    // should only happen on 0 address
    return bits;
}
} // namespace allo::detail
