#pragma once
#include "allo/detail/abstracts.h"

namespace allo {

/// A very simple allocator which takes in a fixed buffer of memory and
/// allocates randomly sized items within that buffer. It has free available as
/// an operation but it does nothing.
class scratch_allocator_t : public detail::abstract_allocator_t
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
    make(bytes_t memory) noexcept
    {
        return make_inner(memory, {});
    }

    inline static zl::res<scratch_allocator_t, AllocationStatusCode>
    make_owned(bytes_t memory,
               detail::abstract_heap_allocator_t &parent) noexcept
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
    make_inner(bytes_t memory,
               zl::opt<detail::abstract_heap_allocator_t &> parent) noexcept;

    /// Add additional bytes to whatever is already available
    allocation_status_t
    try_make_space_for_at_least(size_t bytes,
                                uint8_t alignment_exponent) noexcept;

    struct destruction_callback_entry_t
    {
        destruction_callback_t callback;
        void *user_data;
    };

    struct M
    {
        bytes_t memory;
        bytes_t available_memory;
        zl::opt<detail::abstract_heap_allocator_t &> parent;
        allo::allocator_properties_t properties;
        const size_t original_size;
        /// 1/log10(original_allocation_size). Used to make sure the size of
        /// memory is always an even integer exponential of the original
        /// allocation size
        const float remap_divisor;
    } m;

    scratch_allocator_t(M &&members) noexcept : m(members)
    {
        m_type = enum_value;
    }
};
} // namespace allo

#ifdef ALLO_HEADER_ONLY
#include "allo/impl/scratch_allocator.h"
#endif
