#pragma once
#include "allo/allocator_interfaces.h"

#ifndef ALLO_NOEXCEPT
#define ALLO_NOEXCEPT noexcept
#endif

namespace allo {

/// A very simple allocator which takes in a fixed buffer of memory and
/// allocates randomly sized items within that buffer. It has free available as
/// an operation but it does nothing.
class scratch_allocator_t : public detail::allocator_t,
                            public detail::freer_t,
                            public detail::stack_reallocator_t,
                            private detail::dynamic_allocator_base_t
{
  public:
    static constexpr detail::AllocatorType enum_value =
        detail::AllocatorType::StackAllocator;

    // can be explicitly constructed from a buffer of existing memory.
    // it will modify this memory, but not free it on destruction.
    inline explicit scratch_allocator_t(zl::slice<uint8_t> memory) noexcept
        : m_memory(memory)
    {
        type = detail::AllocatorType::ScratchAllocator;
    }

    // no need to do anything upon destruction since this is non-owning
    ~scratch_allocator_t() = default;

    [[nodiscard]] allocation_result_t
    alloc_bytes(size_t bytes, size_t alignment, size_t typehash);

    [[nodiscard]] allocation_result_t
    realloc_bytes(zl::slice<uint8_t> mem, size_t new_size, size_t typehash);

    /// Freeing with a scratch allocator is a no-op
    inline constexpr allocation_status_t free_bytes(zl::slice<uint8_t> /*mem*/,
                                                    size_t /*typehash*/)
    {
        return AllocationStatusCode::Okay;
    }

    [[nodiscard]] const allocator_properties_t &properties() const;

  private:
    zl::slice<uint8_t> m_memory;
};
} // namespace allo

#ifdef ALLO_HEADER_ONLY
#include "allo/impl/scratch_allocator.h"
#endif
