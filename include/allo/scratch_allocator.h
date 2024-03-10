#pragma once
#include "allo/abstracts.h"

#ifndef ALLO_NOEXCEPT
#define ALLO_NOEXCEPT noexcept
#endif

namespace allo {

/// A very simple allocator which takes in a fixed buffer of memory and
/// allocates randomly sized items within that buffer. It has free available as
/// an operation but it does nothing.
class scratch_allocator_t : private detail::dynamic_allocator_base_t
{
  public:
    static constexpr detail::AllocatorType enum_value =
        detail::AllocatorType::ScratchAllocator;

    // can be explicitly constructed from a buffer of existing memory.
    // it will modify this memory, but not free it on destruction.
    inline explicit scratch_allocator_t(zl::slice<uint8_t> memory) noexcept
        : m_memory(memory)
    {
        type = enum_value;
    }

    // no need to do anything upon destruction since this is non-owning
    ~scratch_allocator_t() = default;

    [[nodiscard]] allocation_result_t alloc_bytes(size_t bytes,
                                                  uint8_t alignment_exponent,
                                                  size_t typehash) noexcept;

    [[nodiscard]] allocation_result_t
    remap_bytes(zl::slice<uint8_t> mem, size_t old_typehash, size_t new_size,
                  size_t new_typehash) noexcept;

    /// Freeing with a scratch allocator is a no-op
    inline constexpr allocation_status_t
    free_bytes(zl::slice<uint8_t> /*mem*/, // NOLINT
               size_t /*typehash*/) noexcept
    {
        return AllocationStatusCode::Okay;
    }

    [[nodiscard]] inline constexpr allocation_status_t
    free_status(zl::slice<uint8_t> mem, size_t typehash) const noexcept
    {
        return AllocationStatusCode::Okay;
    }

    [[nodiscard]] const allocator_properties_t &properties() const;

    allocation_status_t
    register_destruction_callback(destruction_callback_t callback,
                                  void *user_data) noexcept;

  private:
    zl::slice<uint8_t> m_memory;
};
} // namespace allo

#ifdef ALLO_HEADER_ONLY
#include "allo/impl/scratch_allocator.h"
#endif
