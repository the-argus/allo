#pragma once
#include "allo/structures/collection.h"

namespace allo::detail {
struct bytes_collection_t
{
    collection_t<bytes_t> buffers;
    size_t current_buffer_size;

    // how much alignment to request from newly allocated buffers
    static constexpr size_t bufferalign = 64;

    // add a new buffer to the list of buffers, and if the bytes needed in the
    // new buffer is larger than the size of the current buffer, grow
    allo::allocation_status_t
    new_buffer_for(detail::abstract_heap_allocator_t& allocator,
                   size_t bytes) noexcept;
};
} // namespace allo::detail
