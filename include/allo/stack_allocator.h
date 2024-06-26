#pragma once
#include "allo/detail/abstracts.h"
#include "allo/detail/destruction_callback.h"
#include "allo/structures/segmented_stack.h"

namespace allo {

class stack_allocator_t : public detail::abstract_stack_allocator_t
{
  private:
    using destruction_callback_entry_t =
        detail::destruction_callback_entry_list_node_t;

    struct M
    {
        bytes_t memory;
        uint8_t* top;
#ifndef ALLO_DISABLE_TYPEINFO
        size_t last_type_hashcode = 0;
#endif
        segmented_stack_t<bytes_t>* blocks = nullptr;
        destruction_callback_entry_t* last_callback = nullptr;
        const size_t original_size;
        any_allocator_t parent;
    } m;

    static constexpr size_t blocks_stack_initial_items = 2;

  public:
    static constexpr detail::AllocatorType enum_value =
        detail::AllocatorType::StackAllocator;

    // cannot be default constructed or copied
    stack_allocator_t() = delete;
    stack_allocator_t(const stack_allocator_t&) = delete;
    stack_allocator_t& operator=(const stack_allocator_t&) = delete;
    // can be moved
    stack_allocator_t(stack_allocator_t&&) noexcept;
    // cannot be move assigned
    stack_allocator_t& operator=(stack_allocator_t&&) = delete;

    // create a stack allocator which takes ownership of a block of memory
    // allocated by the parent.
    inline static stack_allocator_t
    make_owning(bytes_t memory,
                detail::abstract_heap_allocator_t& parent) noexcept
    {
        return make_inner(memory, parent);
    }

    // create a stack allocator which allocates into a given block of memory.
    // it does not own the memory. it cannot allocate new buffers because it
    // has no parent.
    inline static stack_allocator_t make(bytes_t memory) noexcept
    {
        return make_inner(memory, {});
    }

    // create a stack allocator which allocates into a given buffer, and can
    // allocate new buffers for theoretically unbounded space, but will not free
    // those buffers upon destruction
    inline static stack_allocator_t
    make(bytes_t memory, detail::abstract_allocator_t& parent) noexcept
    {
        return make_inner(memory, parent);
    }

    ~stack_allocator_t() noexcept;

    [[nodiscard]] allocation_result_t alloc_bytes(size_t bytes,
                                                  uint8_t alignment_exponent,
                                                  size_t typehash) noexcept;

    [[nodiscard]] allocation_result_t remap_bytes(bytes_t mem,
                                                  size_t old_typehash,
                                                  size_t new_size,
                                                  size_t new_typehash) noexcept;

    allocation_status_t free_bytes(bytes_t mem, size_t typehash) noexcept;

    [[nodiscard]] allocation_status_t
    free_status(bytes_t mem, size_t typehash) const noexcept;

    allocation_status_t
    register_destruction_callback(destruction_callback_t callback,
                                  void* user_data) noexcept;

    inline stack_allocator_t(M&& members) noexcept : m(members)
    {
        m_type = enum_value;
    }

  private:
    /// Allocate stuff with no typing
    [[nodiscard]] void* raw_alloc(size_t align, size_t typesize) noexcept;

    static stack_allocator_t make_inner(bytes_t memory,
                                        any_allocator_t parent) noexcept;

    /// the information placed underneath every allocation in the stack
    struct previous_state_t
    {
#ifndef ALLO_DISABLE_TYPEINFO
        size_t type_hashcode;
#endif
        uint8_t* stack_top;
    };

    [[nodiscard]] inline constexpr size_t bytes_remaining() const
    {
        return m.memory.end().ptr() - m.top;
    }

    [[nodiscard]] allocation_status_t
    try_make_space_for_at_least(size_t bytes,
                                uint8_t alignment_exponent) noexcept;

    [[nodiscard]] size_t
    round_up_to_valid_buffersize(size_t needed_bytes) const noexcept;

    /// Common logic shared between freeing functions
    [[nodiscard]] zl::res<previous_state_t&, AllocationStatusCode>
    free_common(bytes_t mem, size_t typehash) const noexcept;
};
} // namespace allo

#ifdef ALLO_HEADER_ONLY
#include "allo/impl/stack_allocator.h"
#endif
