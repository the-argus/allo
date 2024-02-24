#pragma once
#ifndef ALLO_HEADER_ONLY
#ifndef ALLO_OVERRIDE_IMPL_INCLUSION_GUARD
#error \
    "Attempt to include allo/impl header file but header-only mode is not enabled."
#endif
#endif

#include "allo/scratch_allocator.h"

#ifdef ALLO_HEADER_ONLY
#ifndef ALLO_FUNC
#define ALLO_FUNC inline
#endif
#else
#define ALLO_FUNC
#endif

namespace allo {

[[nodiscard]] allocation_result_t
scratch_allocator_t::realloc_bytes(zl::slice<uint8_t> mem, size_t old_typehash,
                                   size_t new_size,
                                   size_t new_typehash) noexcept
{
    return AllocationStatusCode::InvalidArgument;
}

[[nodiscard]] ALLO_FUNC allocation_result_t scratch_allocator_t::alloc_bytes(
    size_t bytes, uint8_t alignment_exponent, size_t typehash) noexcept
{
    return AllocationStatusCode::InvalidArgument;
}

ALLO_FUNC allocation_status_t
scratch_allocator_t::register_destruction_callback(
    destruction_callback_t callback, void *user_data) noexcept
{
    return AllocationStatusCode::InvalidArgument;
}
} // namespace allo
