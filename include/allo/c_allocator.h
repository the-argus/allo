#pragma once

#include "allo/allocator_interfaces.h"

namespace allo {

class c_allocator_t : private detail::dynamic_allocator_base_t,
                      public detail::allocator_t,
                      public detail::freer_t,
                      public detail::reallocator_t,
                      public detail::destruction_callback_provider_t
{
  public:
    static constexpr detail::AllocatorType enum_value =
        detail::AllocatorType::CAllocator;

    inline explicit c_allocator_t() noexcept { type = enum_value; }

    [[nodiscard]] allocation_result_t
    alloc_bytes(size_t bytes, uint8_t alignment_exponent, size_t typehash);

    [[nodiscard]] allocation_result_t realloc_bytes(zl::slice<uint8_t> mem,
                                                    size_t old_typehash,
                                                    size_t new_size,
                                                    size_t new_typehash);

    allocation_status_t free_bytes(zl::slice<uint8_t> mem, size_t typehash);

    [[nodiscard]] const allocator_properties_t &properties() const;

    allocation_status_t
    register_destruction_callback(destruction_callback_t callback,
                                  void *user_data) noexcept;
};
} // namespace allo

#ifdef ALLO_HEADER_ONLY
#include "allo/impl/c_allocator.h"
#endif
