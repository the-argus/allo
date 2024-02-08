#pragma once
#include "ziglike/res.h"
#include "ziglike/slice.h"
#include "ziglike/status.h"
#include <cstddef>
#include <cstdint>
#include <cstdlib>

namespace allo::interfaces {


using allocation_type_t = uint16_t;

enum class status_code_e : uint8_t
{
    Okay,
    ResultReleased,
    AlreadyFreed,
    OOM,
};

using allocation_status_t = lib::status_t<status_code_e>;

/// May either be a successfull allocation, or a status code failure. Check by
/// calling okay(), and if okay, call release() to get the allocated memory.
using allocation_result_t = lib::result_t<lib::slice_t<uint8_t>, status_code_e>;

using random_free_function = allocation_status_t (*)(
    AllocationType type, void *block, size_t size_in_bytes) TESTING_NOEXCEPT;
using random_alloc_function =
    allocation_result_t (*)(AllocationType type, size_t member_size,
                            size_t num_members) TESTING_NOEXCEPT;
using random_realloc_function = allocation_result_t (*)(
    AllocationType type, void *existing_block, size_t size_in_bytes,
    size_t requested_size_in_bytes) TESTING_NOEXCEPT;

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
