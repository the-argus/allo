#pragma once

#include "allo/allocator_interfaces.h"

namespace allo {

class c_allocator_t : public detail::allocator_t,
                      public detail::freer_t,
                      public detail::reallocator_t,
                      private detail::dynamic_allocator_base_t
{
  public:
    static constexpr detail::AllocatorType enum_value =
        detail::AllocatorType::CAllocator;

    inline explicit c_allocator_t() noexcept { type = enum_value; }

    [[nodiscard]] allocation_result_t
    alloc_bytes(size_t bytes, size_t alignment, size_t typehash);

    [[nodiscard]] allocation_result_t
    realloc_bytes(zl::slice<uint8_t> mem, size_t new_size, size_t typehash);

    allocation_status_t free_bytes(zl::slice<uint8_t> mem, size_t typehash);

    [[nodiscard]] const allocator_properties_t &properties() const;
};
} // namespace allo

#ifdef ALLO_HEADER_ONLY
#include "allo/impl/c_allocator.h"
#endif