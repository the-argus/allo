#pragma once

#include "allo/allocator_interfaces.h"

namespace allo {

class heap_allocator_t : public detail::allocator_t,
                         public detail::freer_t,
                         public detail::reallocator_t,
                         private detail::dynamic_allocator_base_t
{
  public:
    static constexpr detail::AllocatorType enum_value =
        detail::AllocatorType::HeapAllocator;

    /// Create a heap allocator which will allocate into a given buffer of
    /// memory
    /// TODO: make "maximum contiguous bytes" account for bookkeeping
    /// information which may take up space in the buffer
    inline explicit heap_allocator_t(const zl::slice<uint8_t> &memory) noexcept
        : m_mem(memory), m_properties(make_properties(
                             memory.size(), memory.size(), max_alignment))
    {
        type = enum_value;
    }

    [[nodiscard]] allocation_result_t
    alloc_bytes(size_t bytes, size_t alignment, size_t typehash);

    [[nodiscard]] allocation_result_t
    realloc_bytes(zl::slice<uint8_t> mem, size_t new_size, size_t typehash);

    allocation_status_t free_bytes(zl::slice<uint8_t> mem, size_t typehash);

    [[nodiscard]] inline constexpr const allocator_properties_t &
    properties() const
    {
        return m_properties;
    }

  private:
    static constexpr size_t max_alignment = 32;
    zl::slice<uint8_t> m_mem;
    allocator_properties_t m_properties;
};
} // namespace allo

#ifdef ALLO_HEADER_ONLY
#include "allo/impl/heap_allocator.h"
#endif
