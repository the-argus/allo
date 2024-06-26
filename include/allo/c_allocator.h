#pragma once

#include "allo/detail/abstracts.h"

namespace allo {

/// Sometimes you want to perform generic allocator operations but have no need
/// of being able to free all of the elements at once. If not for the former
/// requirement, you could just use malloc and free directly. To solve that,
/// this allocator provides an allo wrapper around malloc and free. Also useful
/// for passing to allocators. Note that the c allocator may not be capable of
/// remapping upon calling realloc and you should use a reservation_allocator
/// allocator if you plan for child allocators to be able to expand their
/// allocations.
/// The C allocator is abstraction-breaking:
///  - it provides a remap_bytes, which doesnt make sense for malloc and always
///  fails
///  - it does not have a mechanism to free all of its allocations.
class c_allocator_t : public detail::abstract_threadsafe_heap_allocator_t
{
  public:
    static constexpr detail::AllocatorType enum_value =
        detail::AllocatorType::CAllocator;

    inline explicit c_allocator_t() noexcept { m_type = enum_value; }

    [[nodiscard]] allocation_result_t alloc_bytes(size_t bytes,
                                                  uint8_t alignment_exponent,
                                                  size_t typehash) noexcept;

    [[nodiscard]] allocation_result_t
    threadsafe_realloc_bytes(bytes_t mem, size_t old_typehash, size_t new_size,
                             size_t new_typehash) noexcept;

    allocation_status_t free_bytes(bytes_t mem, size_t typehash) noexcept;

    [[nodiscard]] inline constexpr allocation_status_t
    free_status(bytes_t /*mem*/, // NOLINT
                size_t /*typehash*/) const noexcept
    {
        return AllocationStatusCode::Okay;
    }

    /// Always returns invalid argument
    inline constexpr allocation_status_t
    register_destruction_callback(destruction_callback_t, void*) noexcept
    {
        return AllocationStatusCode::InvalidArgument;
    }

    /// Always returns invalid argument
    [[nodiscard]] inline allocation_result_t
    remap_bytes(bytes_t, size_t, size_t, size_t) noexcept
    {
        return AllocationStatusCode::InvalidArgument;
    }
};
} // namespace allo

#ifdef ALLO_HEADER_ONLY
#include "allo/impl/c_allocator.h"
#endif
