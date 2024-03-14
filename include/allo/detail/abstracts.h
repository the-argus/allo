#pragma once
#include "allo/detail/forward_decls.h"
#include "allo/properties.h"
#include "allo/status.h"
#include <type_traits>

namespace allo {
template <typename Allocator>
zl::res<zl::slice<uint8_t>, AllocationStatusCode>
realloc_bytes(Allocator &allocator, zl::slice<uint8_t> original,
              size_t new_size) noexcept;
namespace detail {
class allocator_common_t;
};
// used by allocator_common_t
using destruction_callback_t = void (*)(void *user_data);
const char *allocator_typename(detail::allocator_common_t) noexcept;
} // namespace allo

namespace allo::detail {
enum class AllocatorType : uint8_t
{
    CAllocator,
    BlockAllocator,
    StackAllocator,
    ScratchAllocator,
    OneshotAllocator,
    ReservationAllocator,
    HeapAllocator,
    MAX_ALLOCATOR_TYPE
};

// all allocators inherit from this
class dynamic_allocator_base_t
{
  public:
    AllocatorType type;
};

template <typename From, typename To> struct can_upcast
{
    using type = std::false_type;
};

class allocator_common_t
{
  protected:
    void *ref;

    inline constexpr explicit allocator_common_t(void *_ref) noexcept
        : ref(_ref)
    {
    }

  public:
    allocator_common_t() = delete;

    allocator_common_t(const allocator_common_t &) noexcept = default;

    template <typename Allocator>
    inline constexpr allocator_common_t(Allocator &allocator) noexcept
        : ref([](Allocator &allocator) -> void * {
              static_assert(
                  detail::can_upcast<Allocator,
                                     allocator_common_t>::type::value,
                  "Given type cannot be converted to a AllocatorDynRef");
              if constexpr (std::is_base_of_v<allocator_common_t, Allocator>) {
                  return allocator.ref;
              } else {
                  return &allocator;
              }
          }(allocator))
    {
    }

    /// Request an allocation for some number of bytes with some alignment, and
    /// providing the typehash. If a non-typed allocator, 0 can be supplied as
    /// the hash.
    [[nodiscard]] allocation_result_t alloc_bytes(size_t bytes,
                                                  uint8_t alignment_exponent,
                                                  size_t typehash) noexcept;

    [[nodiscard]] const allocator_properties_t &properties() const noexcept;

    [[nodiscard]] allocation_status_t
    register_destruction_callback(destruction_callback_t callback,
                                  void *user_data) noexcept;

    friend const char *
        allo::allocator_typename(detail::allocator_common_t) noexcept;

    template <typename Allocator>
    friend zl::res<zl::slice<uint8_t>, AllocationStatusCode>
    allo::realloc_bytes(Allocator &allocator, zl::slice<uint8_t> original,
                        size_t new_size) noexcept;
};

class dynamic_stack_allocator_t : public allocator_common_t
{
  protected:
    inline constexpr explicit dynamic_stack_allocator_t(void *_ref) noexcept
        : allocator_common_t(_ref)
    {
    }

  public:
    [[nodiscard]] allocation_result_t remap_bytes(zl::slice<uint8_t> mem,
                                                  size_t old_typehash,
                                                  size_t new_size,
                                                  size_t new_typehash) noexcept;
    allocation_status_t free_bytes(zl::slice<uint8_t> mem,
                                   size_t typehash) noexcept;
    /// Returns Okay if the free of the given memory would succeed, otherwise
    /// returns the error that would be returned if you tried to free.
    [[nodiscard]] allocation_status_t free_status(zl::slice<uint8_t> mem,
                                                  size_t typehash) noexcept;

    template <typename Allocator>
    inline constexpr dynamic_stack_allocator_t(Allocator &allocator) noexcept
        : allocator_common_t([](Allocator &allocator) -> void * {
              static_assert(detail::can_upcast<Allocator,
                                               dynamic_stack_allocator_t>::type,
                            "The give allocator type cannot be converted to a "
                            "StackAllocatorDynRef.");
              if constexpr (std::is_base_of_v<allocator_common_t, Allocator>) {
                  return allocator.ref;
              } else {
                  return &allocator;
              }
          }(allocator))
    {
    }

    template <>
    inline constexpr dynamic_stack_allocator_t( // NOLINT
        dynamic_stack_allocator_t &other) noexcept
        : allocator_common_t(other.ref)
    {
    }
};

// heap allocator is a semantic difference, functionally you can try to call
// all the same functions. it's just that in a  heap allocator, you can
// reallocate and free in any order
class dynamic_heap_allocator_t : public dynamic_stack_allocator_t
{
  public:
    template <typename Allocator>
    inline constexpr dynamic_heap_allocator_t(Allocator &allocator) noexcept
        : dynamic_stack_allocator_t([](Allocator &allocator) -> void * {
              static_assert(
                  detail::can_upcast<Allocator,
                                     dynamic_heap_allocator_t>::type::value,
                  "The give allocator type cannot be converted to a "
                  "HeapAllocatorDynRef.");
              if constexpr (std::is_base_of_v<allocator_common_t, Allocator>) {
                  return allocator.ref;
              } else {
                  return &allocator;
              }
          }(allocator))
    {
    }

    template <>
    inline constexpr dynamic_heap_allocator_t( // NOLINT
        dynamic_heap_allocator_t &other) noexcept
        : dynamic_stack_allocator_t(other.ref)
    {
    }
};

// partial template specialization for allocator_common_t: all allocators can
// be upcast to this
template <typename From> struct can_upcast<From, allocator_common_t>
{
    using type = std::conditional_t<
        std::is_base_of_v<detail::dynamic_allocator_base_t, From> ||
            std::is_base_of_v<allocator_common_t, From>,
        std::true_type, std::false_type>;
};

template <> struct can_upcast<allocator_common_t, allocator_common_t>
{
    using type = std::true_type;
};

// specialization where you are upcasting something to itself
template <typename From> struct can_upcast<From, From>
{
    using type = std::true_type;
};

#define ALLO_DETAIL_ALLOW_UPCAST(from, to)  \
    template <> struct can_upcast<from, to> \
    {                                       \
        using type = std::true_type;        \
    };

// specializations for upcasting dynamic references to other dynamic references
ALLO_DETAIL_ALLOW_UPCAST(dynamic_heap_allocator_t, dynamic_stack_allocator_t)

// c allocator
ALLO_DETAIL_ALLOW_UPCAST(c_allocator_t, dynamic_stack_allocator_t)
ALLO_DETAIL_ALLOW_UPCAST(c_allocator_t, dynamic_heap_allocator_t)
// block allocator
ALLO_DETAIL_ALLOW_UPCAST(block_allocator_t, dynamic_stack_allocator_t)
ALLO_DETAIL_ALLOW_UPCAST(block_allocator_t, dynamic_heap_allocator_t)
// stack allocator
ALLO_DETAIL_ALLOW_UPCAST(stack_allocator_t, dynamic_stack_allocator_t)
// oneshot allocator
ALLO_DETAIL_ALLOW_UPCAST(oneshot_allocator_t, dynamic_stack_allocator_t)
ALLO_DETAIL_ALLOW_UPCAST(oneshot_allocator_t, dynamic_heap_allocator_t)
// reservation allocator
ALLO_DETAIL_ALLOW_UPCAST(reservation_allocator_t, dynamic_stack_allocator_t)
ALLO_DETAIL_ALLOW_UPCAST(reservation_allocator_t, dynamic_heap_allocator_t)
// heap allocator
ALLO_DETAIL_ALLOW_UPCAST(heap_allocator_t, dynamic_stack_allocator_t)
ALLO_DETAIL_ALLOW_UPCAST(heap_allocator_t, dynamic_heap_allocator_t)
#undef ALLO_DETAIL_ALLOW_UPCAST

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

#ifdef ALLO_HEADER_ONLY
#include "allo/impl/abstracts.h"
#endif
