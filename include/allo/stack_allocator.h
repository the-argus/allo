#pragma once
#include "allo/detail/abstracts.h"

namespace allo {

class stack_allocator_t : public detail::abstract_stack_allocator_t
{
  private:
    struct M
    {
        zl::opt<detail::abstract_heap_allocator_t &> parent;
        bytes_t memory;
        bytes_t available_memory;
        size_t last_type_hashcode = 0;
        allocator_properties_t properties;
    } m;

  public:
    static constexpr detail::AllocatorType enum_value =
        detail::AllocatorType::StackAllocator;

    // cannot be default constructed or copied
    stack_allocator_t() = delete;
    stack_allocator_t(const stack_allocator_t &) = delete;
    stack_allocator_t &operator=(const stack_allocator_t &) = delete;

    inline static zl::res<stack_allocator_t, AllocationStatusCode>
    make_owned(bytes_t memory,
               detail::abstract_heap_allocator_t& parent) noexcept
    {
        return make_inner(memory, parent);
    }

    // create a stack allocator which allocates into a given block of memory.
    inline static zl::res<stack_allocator_t, AllocationStatusCode>
    make(bytes_t memory) noexcept
    {
        return make_inner(memory, {});
    }

    // can be moved
    stack_allocator_t(stack_allocator_t &&) noexcept;

    // cannot be move assigned
    stack_allocator_t &operator=(stack_allocator_t &&) = delete;

    // owns its memory
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

    [[nodiscard]] const allocator_properties_t &properties() const noexcept;

    allocation_status_t
    register_destruction_callback(destruction_callback_t callback,
                                  void *user_data) noexcept;

    inline explicit stack_allocator_t(M &&members) noexcept : m(members)
    {
        m_type = enum_value;
    }

  private:
    /// Allocate stuff with no typing
    void *raw_alloc(size_t align, size_t typesize) noexcept;

    static constexpr double reallocation_ratio = 1.5;
    allocation_status_t realloc() noexcept;

    static zl::res<stack_allocator_t, AllocationStatusCode>
    make_inner(bytes_t memory,
               zl::opt<detail::abstract_heap_allocator_t &> parent) noexcept;

    /// the information placed underneath every allocation in the stack
    struct previous_state_t
    {
        size_t stack_top;
        size_t type_hashcode;
    };

    struct destruction_callback_entry_t
    {
        destruction_callback_t callback;
        void *user_data;
    };

    /// Common logic shared between freeing functions
    [[nodiscard]] zl::res<previous_state_t &, AllocationStatusCode>
    free_common(bytes_t mem, size_t typehash) const noexcept;
};
} // namespace allo

#ifdef ALLO_HEADER_ONLY
#include "allo/impl/stack_allocator.h"
#endif
