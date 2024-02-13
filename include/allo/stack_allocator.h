#pragma once
#include "allo/allocator_interfaces.h"

#ifndef ALLO_NOEXCEPT
#define ALLO_NOEXCEPT noexcept
#endif

namespace allo {

/// A very simple allocator which takes in a fixed buffer of memory and
/// allocates randomly sized items within that buffer. They can only be freed in
/// the opposite order that they were allocated.
class stack_allocator_t : private detail::dynamic_allocator_base_t,
                          public detail::allocator_t,
                          public detail::stack_freer_t,
                          public detail::stack_reallocator_t
{
  public:
    static constexpr detail::AllocatorType enum_value =
        detail::AllocatorType::StackAllocator;

    // cannot be default constructed or copied
    stack_allocator_t() = delete;
    stack_allocator_t(const stack_allocator_t &) = delete;
    stack_allocator_t &operator=(const stack_allocator_t &) = delete;

    // can be explicitly constructed from a buffer of existing memory.
    // it will modify this memory, but not free it on destruction.
    explicit stack_allocator_t(zl::slice<uint8_t> memory) ALLO_NOEXCEPT;

    // can be moved
    stack_allocator_t(stack_allocator_t &&) noexcept;
    stack_allocator_t &operator=(stack_allocator_t &&) noexcept;

    // no need to do anything upon destruction since this is non-owning
    ~stack_allocator_t() = default;

    /// Zero all the memory in this stack allocator's buffer
    void zero() ALLO_NOEXCEPT;

    [[nodiscard]] allocation_result_t
    alloc_bytes(size_t bytes, size_t alignment, size_t typehash);

    [[nodiscard]] allocation_result_t
    realloc_bytes(zl::slice<uint8_t> mem, size_t new_size, size_t typehash);

    allocation_status_t free_bytes(zl::slice<uint8_t> mem, size_t typehash);

    [[nodiscard]] allocation_status_t free_status(zl::slice<uint8_t> mem,
                                                  size_t typehash) const;

    [[nodiscard]] const allocator_properties_t &properties() const;

  private:
    /// Allocate stuff with no typing
    void *raw_alloc(size_t align, size_t typesize) ALLO_NOEXCEPT;
    /// Free something
    void *inner_free(size_t align, size_t typesize, void *item) ALLO_NOEXCEPT;
    /// the information placed underneath every allocation in the stack
    struct previous_state_t
    {
        size_t memory_available;
        size_t type_hashcode;
    };

    zl::slice<uint8_t> m_memory;
    size_t m_first_available = 0;
    size_t m_last_type = 0;
    allocator_properties_t m_properties;
};
} // namespace allo

#ifdef ALLO_HEADER_ONLY
#include "allo/impl/stack_allocator.h"
#endif
