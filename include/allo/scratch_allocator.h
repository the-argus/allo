#pragma once
#include "allo/detail/abstracts.h"

namespace allo {

/// A very simple allocator which takes in a fixed buffer of memory and
/// allocates randomly sized items within that buffer. It has free available as
/// an operation but it does nothing.
class scratch_allocator_t : private detail::dynamic_allocator_base_t
{
  public:
    static constexpr detail::AllocatorType enum_value =
        detail::AllocatorType::ScratchAllocator;

    scratch_allocator_t() = delete;
    scratch_allocator_t(const scratch_allocator_t &) = delete;
    scratch_allocator_t &operator=(const scratch_allocator_t &) = delete;
    scratch_allocator_t(scratch_allocator_t &&) noexcept;
    scratch_allocator_t &operator=(scratch_allocator_t &&) = delete;

    inline static zl::res<scratch_allocator_t, AllocationStatusCode>
    make(zl::slice<uint8_t> memory) noexcept
    {
        return make_inner(memory, {});
    }

    inline static zl::res<scratch_allocator_t, AllocationStatusCode>
    make_owned(zl::slice<uint8_t> memory,
               detail::dynamic_heap_allocator_t parent) noexcept
    {
        return make_inner(memory, parent);
    }

    ~scratch_allocator_t() noexcept;

    [[nodiscard]] allocation_result_t alloc_bytes(size_t bytes,
                                                  uint8_t alignment_exponent,
                                                  size_t typehash) noexcept;

    [[nodiscard]] const allocator_properties_t &properties() const;

    allocation_status_t
    register_destruction_callback(destruction_callback_t callback,
                                  void *user_data) noexcept;

  private:
    static zl::res<scratch_allocator_t, AllocationStatusCode>
    make_inner(zl::slice<uint8_t> memory,
               zl::opt<detail::dynamic_heap_allocator_t> parent) noexcept;

    [[nodiscard]] inline uint8_t *top() const noexcept
    {
        return m.memory.data() + m.top;
    }

    struct M
    {
        zl::slice<uint8_t> memory;
        zl::opt<detail::dynamic_heap_allocator_t> parent;
        allo::allocator_properties_t properties;
        size_t top;
    } m;

    scratch_allocator_t(M &&members) noexcept : m(members)
    {
        type = enum_value;
    }
};
} // namespace allo

#ifdef ALLO_HEADER_ONLY
#include "allo/impl/scratch_allocator.h"
#endif
