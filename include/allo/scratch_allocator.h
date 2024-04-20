#pragma once
#include "allo/detail/abstracts.h"
#include "allo/structures/any_allocator.h"
#include "allo/structures/segmented_stack.h"

namespace allo {

/// A very simply allocator which can allocate items in first-in, never-out
/// order. You cannot free with this allocator, except for allowing it to be
/// destructed, in which case it will free all of its allocations.
class scratch_allocator_t : public detail::abstract_allocator_t
{
  private:
    struct destruction_callback_entry_t
    {
        destruction_callback_t callback;
        void *user_data;
        destruction_callback_entry_t *prev = nullptr;
    };

    struct M
    {
        bytes_t memory;
        uint8_t *top;
        segmented_stack_t<bytes_t> *blocks = nullptr;
        destruction_callback_entry_t *last_callback = nullptr;
        const size_t original_size;
        any_allocator_t parent;
        /// 1/log10(original_allocation_size). Used to make sure the size of
        /// memory is always an even integer exponential of the original
        /// allocation size
        const float remap_divisor;
    } m;

    static constexpr size_t blocks_stack_initial_items = 2;

  public:
    static constexpr detail::AllocatorType enum_value =
        detail::AllocatorType::ScratchAllocator;

    scratch_allocator_t() = delete;
    scratch_allocator_t(const scratch_allocator_t &) = delete;
    scratch_allocator_t &operator=(const scratch_allocator_t &) = delete;
    scratch_allocator_t(scratch_allocator_t &&) noexcept;
    scratch_allocator_t &operator=(scratch_allocator_t &&) = delete;

    inline static scratch_allocator_t make(bytes_t memory) noexcept
    {
        return make_inner(memory, {});
    }

    inline static scratch_allocator_t
    make(bytes_t memory, detail::abstract_allocator_t &parent) noexcept
    {
        return make_inner(memory, parent);
    }

    inline static scratch_allocator_t
    make_owned(bytes_t memory,
               detail::abstract_heap_allocator_t &parent) noexcept
    {
        return make_inner(memory, parent);
    }

    ~scratch_allocator_t() noexcept;

    [[nodiscard]] allocation_result_t alloc_bytes(size_t bytes,
                                                  uint8_t alignment_exponent,
                                                  size_t typehash) noexcept;

    allocation_status_t
    register_destruction_callback(destruction_callback_t callback,
                                  void *user_data) noexcept;

    scratch_allocator_t(M &&members) noexcept : m(members)
    {
        m_type = enum_value;
    }

  private:
    static scratch_allocator_t make_inner(bytes_t memory,
                                          any_allocator_t parent) noexcept;

    /// Add additional bytes to whatever is already available
    allocation_status_t
    try_make_space_for_at_least(size_t bytes,
                                uint8_t alignment_exponent) noexcept;
};
} // namespace allo

#ifdef ALLO_HEADER_ONLY
#include "allo/impl/scratch_allocator.h"
#endif
