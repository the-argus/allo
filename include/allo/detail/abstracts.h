#pragma once
#include "allo/properties.h"
#include "allo/status.h"
#include <type_traits>

namespace allo {
using destruction_callback_t = void (*)(void *user_data);
}

namespace allo::detail {

enum class AllocatorType : uint8_t
{
    CAllocator,
    BlockAllocator,
    StackAllocator,
    ScratchAllocator,
    ReservationAllocator,
    HeapAllocator,
    MAX_ALLOCATOR_TYPE
};

class abstract_allocator_t
{
  protected:
    AllocatorType m_type; // NOLINT
    abstract_allocator_t() = default;

  public:
    abstract_allocator_t(const abstract_allocator_t &) = delete;
    abstract_allocator_t &operator=(const abstract_allocator_t &) = delete;
    abstract_allocator_t(abstract_allocator_t &&) = delete;
    abstract_allocator_t &operator=(abstract_allocator_t &&) = delete;

    [[nodiscard]] inline constexpr AllocatorType type() const noexcept
    {
        return m_type;
    }

    [[nodiscard]] inline constexpr const char *name() const noexcept
    {
        using Type = detail::AllocatorType;
        switch (type()) {
        case Type::CAllocator:
            return "c_allocator_t";
        case Type::BlockAllocator:
            return "block_allocator_t";
        case Type::StackAllocator:
            return "stack_allocator_t";
        case Type::ScratchAllocator:
            return "scratch_allocator_t";
        case Type::ReservationAllocator:
            return "reservation_allocator_t";
        case Type::HeapAllocator:
            return "heap_allocator_t";
        default:
            return "<unknown allocator>";
        }
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

class abstract_stack_allocator_t : public abstract_allocator_t
{
  public:
    [[nodiscard]] allocation_result_t remap_bytes(bytes_t mem,
                                                  size_t old_typehash,
                                                  size_t new_size,
                                                  size_t new_typehash) noexcept;

    allocation_status_t free_bytes(bytes_t mem, size_t typehash) noexcept;

    /// Returns Okay if the free of the given memory would succeed, otherwise
    /// returns the error that would be returned if you tried to free.
    [[nodiscard]] allocation_status_t free_status(bytes_t mem,
                                                  size_t typehash) noexcept;
};

// heap allocator is a semantic difference, functionally you can try to call
// all the same functions. it's just that in a  heap allocator, you can
// reallocate and free in any order
class abstract_heap_allocator_t : public abstract_stack_allocator_t
{};

// A heap allocator which is also threadsafe.
class abstract_threadsafe_heap_allocator_t : public abstract_heap_allocator_t
{
    [[nodiscard]] allocation_result_t
    threadsafe_realloc_bytes(bytes_t mem, size_t old_typehash, size_t new_size,
                             size_t new_typehash) noexcept;
};

template <typename T> struct is_abstract
{
    using type = std::false_type;
};

#define ALLO_MARK_ABSTRACT(_type)         \
    template <> struct is_abstract<_type> \
    {                                     \
        using type = std::true_type;      \
    };

ALLO_MARK_ABSTRACT(abstract_allocator_t)
ALLO_MARK_ABSTRACT(abstract_stack_allocator_t)
ALLO_MARK_ABSTRACT(abstract_heap_allocator_t)
ALLO_MARK_ABSTRACT(abstract_threadsafe_heap_allocator_t)
#undef ALLO_MARK_ABSTRACT

template <typename T>
inline constexpr bool is_allocator = std::is_base_of_v<abstract_allocator_t, T>;

template <typename T>
inline constexpr bool is_freer =
    std::is_base_of_v<abstract_stack_allocator_t, T>;

template <typename T>
inline constexpr bool is_reallocator =
    std::is_base_of_v<abstract_stack_allocator_t, T>;

template <typename T>
inline constexpr bool is_real_allocator =
    is_allocator<T> && !detail::is_abstract<T>::type::value;

} // namespace allo::detail

#ifdef ALLO_HEADER_ONLY
#include "allo/impl/abstracts.h"
#endif
