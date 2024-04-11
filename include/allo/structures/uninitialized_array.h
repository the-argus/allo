#pragma once
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace allo {
/// An array of items
template <typename T, size_t n> class uninitialized_array_t
{
  public:
    using type = T;
    using value_type = T;
    static constexpr size_t num_items = n;
    static constexpr size_t bytes = sizeof(T) * num_items;

    [[nodiscard]] inline constexpr size_t size() const noexcept
    {
        return num_items;
    }

    [[nodiscard]] inline constexpr T *data() noexcept
    {
        return reinterpret_cast<T *>(contents);
    }

  private:
    struct invalid_type
    {
        invalid_type() = delete;
        ~invalid_type() = delete;
    };

    static constexpr bool can_be_eight_byte_aligned =
        alignof(T) <= alignof(uint64_t);

    static_assert(can_be_eight_byte_aligned,
                  "Cannot create an uninitialized_array_t of a type which is "
                  "more than 8 byte aligned.");

    // NOTE: this could also be achieved with a compiler pragma probably, to
    // just align the array that this contains
    using aligned_int = std::conditional_t<
        alignof(uint8_t) == alignof(T), uint8_t,
        std::conditional_t<
            alignof(uint16_t) == alignof(T), uint16_t,
            std::conditional_t<alignof(uint32_t) == alignof(T), uint32_t,
                               std::conditional_t<can_be_eight_byte_aligned,
                                                  uint64_t, invalid_type>>>>;

    static constexpr bool divisible = ((sizeof(aligned_int) - 1) & bytes) == 0;

    static constexpr size_t items_needed =
        (bytes / sizeof(aligned_int)) + (divisible ? 1 : 0);

    static_assert(items_needed * sizeof(aligned_int) >= bytes,
                  "Incorrectly calculated number of ints needed to have enough "
                  "space for given number of items");

    // NOLINTNEXTLINE
    aligned_int contents[items_needed];
};
}; // namespace allo
