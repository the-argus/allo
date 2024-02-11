#pragma once

#ifndef ALLO_HEADER_ONLY
#ifndef ALLO_OVERRIDE_IMPL_INCLUSION_GUARD
#error \
    "Attempt to include allo/impl header file but header-only mode is not enabled."
#endif
#endif

#include "allo/block_allocator.h"

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
/// TODO: implement block allocator
///

ALLO_FUNC allocation_result_t block_allocator_t::alloc_bytes(
    size_t /*bytes*/, size_t /*alignment*/, size_t /*typehash*/)
{
    return AllocationStatusCode::InvalidArgument;
}

ALLO_FUNC allocation_result_t block_allocator_t::realloc_bytes(
    zl::slice<uint8_t> /*mem*/, size_t /*new_size*/, size_t /*typehash*/)
{
    return AllocationStatusCode::InvalidArgument;
}

ALLO_FUNC allocation_status_t
block_allocator_t::free_bytes(zl::slice<uint8_t> /*mem*/, size_t /*typehash*/)
{
    return AllocationStatusCode::InvalidArgument;
}
} // namespace allo
