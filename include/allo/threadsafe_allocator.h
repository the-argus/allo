#pragma once

#include "allo/allocator_interfaces.h"

namespace allo {

class threadsafe_allocator_t
    : private detail::threadsafe_dynamic_allocator_base_t,
      public detail::allocator_t,
      public detail::freer_t,
      public detail::reallocator_t,
      public detail::destruction_callback_provider_t
{
  public:
    static constexpr detail::AllocatorType enum_value =
        detail::AllocatorType::ThreadsafeAllocator;

    template <typename Allocator>
    inline threadsafe_allocator_t(Allocator &parent,
                                  const zl::slice<uint8_t> &memory) noexcept
        : m_mem(memory),
          m_properties(make_properties(memory.size(), max_alignment))
    {
        type = enum_value;
    }

    [[nodiscard]] allocation_result_t
    alloc_bytes(size_t bytes, size_t alignment, size_t typehash);

    [[nodiscard]] allocation_result_t realloc_bytes(zl::slice<uint8_t> mem,
                                                    size_t old_typehash,
                                                    size_t new_size,
                                                    size_t new_typehash);

    allocation_status_t free_bytes(zl::slice<uint8_t> mem, size_t typehash);

    [[nodiscard]] inline constexpr const allocator_properties_t &
    properties() const
    {
        return m_properties;
    }

    allocation_status_t
    register_destruction_callback(destruction_callback_t callback,
                                  void *user_data) noexcept;

    template <typename ChildAllocator>
    inline zl::res<ChildAllocator, AllocationStatusCode> branch() noexcept
    {
    }

  private:
    static constexpr size_t max_alignment = 32;
    zl::slice<uint8_t> m_mem;
    allocator_properties_t m_properties;
};
} // namespace allo

#ifdef ALLO_HEADER_ONLY
#include "allo/impl/threadsafe_allocator.h"
#endif
