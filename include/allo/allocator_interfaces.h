#pragma once
#include "ziglike/res.h"
#include "ziglike/slice.h"
#include "ziglike/status.h"
#include <cstddef>
#include <cstdint>
#include <cstdlib>

namespace allo::interfaces {

using allocation_type_t = uint16_t;

enum class AllocationStatusCode : uint8_t
{
    Okay,
    ResultReleased,
    AlreadyFreed,
    OOM,
};

using allocation_status_t = zl::status<AllocationStatusCode>;

/// May either be a successfull allocation, or a status code failure. Check by
/// calling okay(), and if okay, call release() to get the allocated memory.
using allocation_result_t = zl::res<zl::slice<uint8_t>, AllocationStatusCode>;

using random_free_function = allocation_status_t (*)(allocation_type_t type,
                                                     void *block,
                                                     size_t size_in_bytes);
using random_alloc_function = allocation_result_t (*)(allocation_type_t type,
                                                      size_t member_size,
                                                      size_t num_members);
using random_realloc_function = allocation_result_t (*)(
    allocation_type_t type, void *existing_block, size_t size_in_bytes,
    size_t requested_size_in_bytes);

/// A set of functions with no context object, just global functions to call to
/// get access to random blocks of memory.
struct random_allocator_t
{
    random_free_function free;
    random_alloc_function alloc;
    random_realloc_function realloc;
};

/// A set of functions which can be used to reallocate and free an existing
/// allocation, but not to create new allocations.
struct random_single_allocation_reallocator_t
{
    random_free_function free;
    random_realloc_function realloc;
};

} // namespace allo::interfaces
