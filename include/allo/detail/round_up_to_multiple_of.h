#pragma once
#include <cstddef>

namespace allo::detail {
template <size_t N> inline constexpr size_t round_up_to_multiple_of(size_t x)
{
    // this code is stolen from rive-cpp
    static_assert(N != 0 && (N & (N - 1)) == 0,
                  "round_up_to_multiple_of only supports powers of 2.");
    return (x + (N - 1)) & ~(N - 1);
}
} // namespace allo::detail
