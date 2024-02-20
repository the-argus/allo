#pragma once

#ifndef ALLO_HEADER_ONLY
#ifndef ALLO_OVERRIDE_IMPL_INCLUSION_GUARD
#error \
    "Attempt to include allo/impl header file but header-only mode is not enabled."
#endif
#endif

#include "allo/threadsafe_allocator.h"

#ifdef ALLO_HEADER_ONLY
#ifndef ALLO_FUNC
#define ALLO_FUNC inline
#endif
#else
#define ALLO_FUNC
#endif

namespace allo {

///
/// NOT IMPLEMENTED !!! --------------------------------------------------------
/// TODO: implement threadsafe allocator
///

ALLO_FUNC allocation_result_t threadsafe_allocator_t::alloc_bytes(
    size_t /*bytes*/, size_t /*alignment*/, size_t /*typehash*/)
{
    return AllocationStatusCode::InvalidArgument;
}

ALLO_FUNC allocation_result_t threadsafe_allocator_t::realloc_bytes(
    zl::slice<uint8_t> mem, size_t old_typehash, size_t new_size,
    size_t new_typehash)
{
    return AllocationStatusCode::InvalidArgument;
}

ALLO_FUNC allocation_status_t threadsafe_allocator_t::free_bytes(
    zl::slice<uint8_t> /*mem*/, size_t /*typehash*/)
{
    return AllocationStatusCode::InvalidArgument;
}

ALLO_FUNC allocation_status_t
threadsafe_allocator_t::register_destruction_callback(
    destruction_callback_t callback, void *user_data) noexcept
{
    return AllocationStatusCode::InvalidArgument;
}
} // namespace allo
