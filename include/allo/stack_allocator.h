#pragma once
#include "allo/allocator_interfaces.h"

#ifndef ALLO_NOEXCEPT
#define ALLO_NOEXCEPT noexcept
#endif

namespace allo {

/// A very simple allocator which takes in a fixed buffer of memory and
/// allocates randomly sized items within that buffer. They can only be freed in
/// the opposite order that they were allocated.
class stack_allocator_t : public allocator_t,
                          public stack_reallocator_t,
                          public stack_freer_t,
                          private detail::dynamic_allocator_base_t
{
  public:
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

    friend class stack_allocator_dynamic_t;

  private:
    /// the information placed underneath every allocation in the stack
    struct previous_state_t
    {
        size_t memory_available;
        size_t type_hashcode;
    };

    void *inner_alloc(size_t align, size_t typesize) ALLO_NOEXCEPT;
    void *raw_alloc(size_t align, size_t typesize) ALLO_NOEXCEPT;
    // inner_free returns a pointer to the space that was just freed, or nullptr
    // on failure
    void *inner_free(size_t align, size_t typesize, void *item) ALLO_NOEXCEPT;

    zl::slice<uint8_t> m_memory;
    size_t m_first_available = 0;
    size_t m_last_type = 0;

    // so that the impls for these interfaces can call back to our inner alloc
    // etc
    friend class allocator_t;
    friend class stack_reallocator_t;
    friend class stack_freer_t;
};
} // namespace allo

#ifdef ALLO_HEADER_ONLY
#include "allo/impl/stack_allocator.h"
#endif
