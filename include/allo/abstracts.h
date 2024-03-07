#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <type_traits>
#include <ziglike/opt.h>
#include <ziglike/res.h>
#include <ziglike/slice.h>
#include <ziglike/status.h>

namespace allo {

using allocation_type_t = uint16_t;

enum class AllocationStatusCode : uint8_t
{
    Okay,
    ResultReleased,
    // the amount of contiguous memory requested is not available
    OOM,
    // private memory inside allocator (bookkeeping data) has been overwritten
    // or it unexpected values
    Corruption,
    // invalid item trying to be freed, usually
    InvalidArgument,
    // you requested a greater alignment than the allocator can provide.
    // guaranteed to not be produced if the allocators' properties meet
    // requirements
    AllocationTooAligned,
    // memory passed in to an allocator function could not concievably be owned
    // by that allocator, either by being outside its bounds or misaligned
    MemoryInvalid,
    // the memory passed in is not MemoryInvalid, but the allocator has some way
    // of keeping track of whether memory has been freed or not, and the one
    // passed in has been freed.
    AlreadyFreed,
    // when using type checking, this indicates that you tried to free as a
    // different type that what was originally allocated
    InvalidType,
};

using allocation_status_t = zl::status<AllocationStatusCode>;

/// May either be a successfull allocation, or a status code failure. Check by
/// calling okay(), and if okay, call release() to get the allocated memory.
using allocation_result_t = zl::res<zl::slice<uint8_t>, AllocationStatusCode>;

struct allocator_requirements_t
{
    // the largest single contiguous allocation you plan on making. null means
    // unbounded, so you require an allocator like malloc which can map virtual
    // memory.
    zl::opt<size_t> maximum_contiguous_bytes;
    // the largest alignment you will require from the allocator.
    uint8_t maximum_alignment = 8;
};

using destruction_callback_t = void (*)(void *user_data);

namespace detail {
class allocator_common_t;

enum class AllocatorType : uint8_t
{
    CAllocator,
    BlockAllocator,
    StackAllocator,
    ScratchAllocator,
    OneshotAllocator,
    HeapAllocator,
    MAX_ALLOCATOR_TYPE
};

// all allocators inherit from this
class dynamic_allocator_base_t
{
  public:
    AllocatorType type;
};
} // namespace detail

class c_allocator_t;
class stack_allocator_t;
class block_allocator_t;
class scratch_allocator_t;
class oneshot_allocator_t;
class heap_allocator_t;

struct allocator_properties_t
{
  public:
    /// Check if the allocator properties meet some given requirements
    [[nodiscard]] inline constexpr bool
    meets(const allocator_requirements_t &requirements) const
    {
        if (!requirements.maximum_contiguous_bytes.has_value()) {
            if (m_maximum_contiguous_bytes != 0) {
                return false;
            }
        } else {
            if (requirements.maximum_contiguous_bytes.value() >
                m_maximum_contiguous_bytes) {
                return false;
            }
        }

        return m_maximum_alignment >= requirements.maximum_alignment;
    }

    /// Useful for testing, as a way of asserting that properties()
    /// getter works for a type and its upcasted reference
    inline constexpr friend bool
    operator==(const allocator_properties_t &a,
               const allocator_properties_t &b) noexcept
    {
        return a.m_maximum_alignment == b.m_maximum_alignment &&
               a.m_maximum_contiguous_bytes == b.m_maximum_contiguous_bytes;
    }

  private:
    allocator_properties_t(const allocator_properties_t &other) = default;
    allocator_properties_t &
    operator=(const allocator_properties_t &other) = default;
    allocator_properties_t(allocator_properties_t &&other) = default;
    allocator_properties_t &operator=(allocator_properties_t &&other) = default;
    // zero means theoretically limitless contiguous allocation is possible
    size_t m_maximum_contiguous_bytes;
    uint8_t m_maximum_alignment;

    inline constexpr allocator_properties_t(size_t max_contiguous_bytes,
                                            uint8_t max_alignment)
        : m_maximum_contiguous_bytes(max_contiguous_bytes),
          m_maximum_alignment(max_alignment)
    {
    }

  public:
    friend class allo::c_allocator_t;
    friend class allo::stack_allocator_t;
    friend class allo::block_allocator_t;
    friend class allo::scratch_allocator_t;
    friend class allo::oneshot_allocator_t;
    friend class allo::heap_allocator_t;
};

namespace detail {
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
                  "Given type cannot be converted to a DynamicAllocatorRef");
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
};

class dynamic_stack_allocator_t : public allocator_common_t
{
  protected:
    inline constexpr explicit dynamic_stack_allocator_t(void *_ref) noexcept
        : allocator_common_t(_ref)
    {
    }

  public:
    [[nodiscard]] allocation_result_t
    realloc_bytes(zl::slice<uint8_t> mem, size_t old_typehash, size_t new_size,
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
                            "DynamicStackAllocatorRef.");
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
  protected:
    inline constexpr explicit dynamic_heap_allocator_t(void *_ref) noexcept
        : dynamic_stack_allocator_t(_ref)
    {
    }

  public:
    template <typename Allocator>
    inline constexpr dynamic_heap_allocator_t(Allocator &allocator) noexcept
        : dynamic_stack_allocator_t([](Allocator &allocator) -> void * {
              static_assert(
                  detail::can_upcast<Allocator,
                                     dynamic_heap_allocator_t>::type::value,
                  "The give allocator type cannot be converted to a "
                  "DynamicHeapAllocatorRef.");
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

/// A "stable" heap allocator is able to realloc without invalidating existing
/// pointers to anything.
class dynamic_stable_heap_allocator_t : public dynamic_heap_allocator_t
{
  public:
    template <typename Allocator>
    inline constexpr dynamic_stable_heap_allocator_t(
        Allocator &allocator) noexcept
        : dynamic_heap_allocator_t([](Allocator &allocator) -> void * {
              static_assert(
                  detail::can_upcast<
                      Allocator, dynamic_stable_heap_allocator_t>::type::value,
                  "The give allocator type cannot be converted to a "
                  "DynamicHeapAllocatorRef.");
              if constexpr (std::is_base_of_v<allocator_common_t, Allocator>) {
                  return allocator.ref;
              } else {
                  return &allocator;
              }
          }(allocator))
    {
    }

    template <>
    inline constexpr dynamic_stable_heap_allocator_t( // NOLINT
        dynamic_stable_heap_allocator_t &other) noexcept
        : dynamic_heap_allocator_t(other.ref)
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
ALLO_DETAIL_ALLOW_UPCAST(dynamic_stable_heap_allocator_t,
                         dynamic_heap_allocator_t)
ALLO_DETAIL_ALLOW_UPCAST(dynamic_stable_heap_allocator_t,
                         dynamic_stack_allocator_t)

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
ALLO_DETAIL_ALLOW_UPCAST(oneshot_allocator_t, dynamic_stable_heap_allocator_t)
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

} // namespace detail

using DynamicAllocatorRef = detail::allocator_common_t;
using DynamicStackAllocatorRef = detail::dynamic_stack_allocator_t;
using DynamicHeapAllocatorRef = detail::dynamic_heap_allocator_t;
using DynamicStableHeapAllocatorRef = detail::dynamic_stable_heap_allocator_t;
} // namespace allo
#ifdef ALLO_HEADER_ONLY
#include "allo/impl/abstracts.h"
#endif
