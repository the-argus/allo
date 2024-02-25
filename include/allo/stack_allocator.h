#pragma once
#include "allo/allocator_interfaces.h"

#ifndef ALLO_NOEXCEPT
#define ALLO_NOEXCEPT noexcept
#endif

namespace allo {

class stack_allocator_t : private detail::dynamic_allocator_base_t,
                          public detail::allocator_t,
                          public detail::stack_freer_t,
                          public detail::destruction_callback_provider_t,
                          public detail::stack_reallocator_t
{
  private:
    struct M
    {
        allocator_with<IRealloc, IFree> &parent;
        zl::slice<uint8_t> memory;
        zl::slice<uint8_t> available_memory;
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

    static zl::res<stack_allocator_t, AllocationStatusCode>
    make(zl::slice<uint8_t> memory,
         allocator_with<IRealloc, IFree> &parent) ALLO_NOEXCEPT;

    // can be moved
    stack_allocator_t(stack_allocator_t &&) noexcept;

    // cannot be move assigned
    stack_allocator_t &operator=(stack_allocator_t &&) = delete;

    // owns its memory
    ~stack_allocator_t() noexcept;

    [[nodiscard]] allocation_result_t alloc_bytes(size_t bytes,
                                                  uint8_t alignment_exponent,
                                                  size_t typehash) noexcept;

    [[nodiscard]] allocation_result_t
    realloc_bytes(zl::slice<uint8_t> mem, size_t old_typehash, size_t new_size,
                  size_t new_typehash) noexcept;

    allocation_status_t free_bytes(zl::slice<uint8_t> mem,
                                   size_t typehash) noexcept;

    [[nodiscard]] allocation_status_t
    free_status(zl::slice<uint8_t> mem, size_t typehash) const noexcept;

    [[nodiscard]] const allocator_properties_t &properties() const noexcept;

    allocation_status_t
    register_destruction_callback(destruction_callback_t callback,
                                  void *user_data) noexcept;

    inline explicit stack_allocator_t(M &&members) noexcept : m(members)
    {
        type = enum_value;
    }

  private:
    /// Allocate stuff with no typing
    void *raw_alloc(size_t align, size_t typesize) noexcept;

    static constexpr double reallocation_ratio = 1.5;
    allocation_status_t realloc() noexcept;

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
    free_common(zl::slice<uint8_t> mem, size_t typehash) const noexcept;
};
} // namespace allo

#ifdef ALLO_HEADER_ONLY
#include "allo/impl/stack_allocator.h"
#endif
