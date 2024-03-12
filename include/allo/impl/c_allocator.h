#pragma once

#ifndef ALLO_HEADER_ONLY
#ifndef ALLO_OVERRIDE_IMPL_INCLUSION_GUARD
#error \
    "Attempt to include allo/impl header file but header-only mode is not enabled."
#endif
#endif

#include "allo/c_allocator.h"

#ifdef ALLO_HEADER_ONLY
#ifndef ALLO_FUNC
#define ALLO_FUNC inline
#endif
#else
#define ALLO_FUNC
#endif

namespace allo {
ALLO_FUNC allocation_result_t c_allocator_t::alloc_bytes(
    size_t bytes, uint8_t alignment_exponent, size_t typehash) noexcept
{
    if (alignment_exponent > 5)
        return AllocationStatusCode::AllocationTooAligned;
    void *newmem = ::malloc(bytes);
    return zl::raw_slice(*reinterpret_cast<uint8_t *>(newmem), bytes);
}

ALLO_FUNC allocation_result_t
c_allocator_t::realloc_bytes(zl::slice<uint8_t> mem, size_t old_typehash,
                             size_t new_size, size_t new_typehash) noexcept
{
    // BUG: c allocator is not really capable of remapping, this should probably
    // always OOM, even if the remapping technically succeeds?
    //
    void *newmem = ::realloc(mem.data(), new_size);
    if (newmem != nullptr && newmem != mem.data()) {
        assert(false);
        return AllocationStatusCode::OOM;
    }
    return zl::raw_slice(*reinterpret_cast<uint8_t *>(newmem), new_size);
}

ALLO_FUNC allocation_status_t
c_allocator_t::free_bytes(zl::slice<uint8_t> mem, size_t typehash) noexcept
{
    ::free(mem.data());
    return AllocationStatusCode::Okay;
}

ALLO_FUNC const allocator_properties_t &
c_allocator_t::properties() const noexcept
{
    static constexpr allocator_properties_t c_allocator_properties =
        allocator_properties_t(0, 32);

    return c_allocator_properties;
}
ALLO_FUNC allocation_status_t c_allocator_t::register_destruction_callback(
    destruction_callback_t callback, void *user_data) noexcept
{
    return AllocationStatusCode::InvalidArgument;
}
}; // namespace allo
