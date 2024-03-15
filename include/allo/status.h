#pragma once
#include <cstdint>
#include <ziglike/res.h>
#include <ziglike/slice.h>
#include <ziglike/status.h>

namespace allo {

using allocation_type_t = uint16_t;

enum class AllocationStatusCode : uint8_t
{
    Okay,
    ResultReleased,
    // the amount of contiguous memory requested is not available
    OOM,
    // private memory inside allocator (bookkeeping data) has been overwritten
    // or it unexpected values
    Corruption,
    // invalid item trying to be freed, usually
    InvalidArgument,
    // you requested a greater alignment than the allocator can provide.
    // guaranteed to not be produced if the allocators' properties meet
    // requirements
    AllocationTooAligned,
    // memory passed in to an allocator function could not concievably be owned
    // by that allocator, either by being outside its bounds or misaligned
    MemoryInvalid,
    // the memory passed in is not MemoryInvalid, but the allocator has some way
    // of keeping track of whether memory has been freed or not, and the one
    // passed in has been freed.
    AlreadyFreed,
    // when using type checking, this indicates that you tried to free as a
    // different type that what was originally allocated
    InvalidType,
    // unknown OS failure returned from system call
    OsErr,
};

using allocation_status_t = zl::status<AllocationStatusCode>;

/// May either be a successfull allocation, or a status code failure. Check by
/// calling okay(), and if okay, call release() to get the allocated memory.
using allocation_result_t = zl::res<zl::slice<uint8_t>, AllocationStatusCode>;
} // namespace allo
