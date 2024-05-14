#pragma once

#include "allo/detail/bytes_collection.h"
#include <ziglike/defer.h>

namespace allo::detail {

allo::allocation_status_t
bytes_collection_t::new_buffer_for(detail::abstract_heap_allocator_t& allocator,
                                   size_t bytes) noexcept
{
    const size_t bytes_rounded_up = ((bytes / bufferalign) + 1) * bufferalign;
    const size_t newbufsize = bytes_rounded_up > current_buffer_size
                                  ? bytes_rounded_up
                                  : current_buffer_size;

    auto maybe_newbuf =
        allo::alloc<uint8_t, detail::abstract_heap_allocator_t, 64>(allocator,
                                                                    newbufsize);
    if (!maybe_newbuf.okay()) [[unlikely]]
        return maybe_newbuf.err();
    bytes_t& newbuf = maybe_newbuf.release_ref();
    zl::defer delbuf(
        [&newbuf, &allocator]() { allo::free(allocator, newbuf); });

    auto status = buffers.try_append(newbuf);
    if (!status.okay())
        return status.err();

    if (newbufsize > current_buffer_size)
        current_buffer_size = newbufsize;

    delbuf.cancel();
    return AllocationStatusCode::Okay;
}
}; // namespace allo::detail
