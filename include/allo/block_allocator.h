#pragma once

#include "allo/detail/abstracts.h"
#include <ziglike/opt.h>

namespace allo {

class block_allocator_t : public detail::abstract_heap_allocator_t
{
  public:
    static constexpr detail::AllocatorType enum_value =
        detail::AllocatorType::BlockAllocator;

    /// Create a block allocator which will attempt to free its memory when it
    /// is destroyed, and will try to remap the memory should it run out of
    /// space.
    inline static zl::res<block_allocator_t, AllocationStatusCode>
    make_owning(bytes_t memory, detail::abstract_heap_allocator_t& parent,
                size_t blocksize) noexcept
    {
        return make_inner(memory, parent, blocksize);
    }

    /// Create a block allocator which allocates into a given block of memory.
    inline static zl::res<block_allocator_t, AllocationStatusCode>
    make(bytes_t memory, size_t blocksize) noexcept
    {
        return make_inner(memory, {}, blocksize);
    }

    [[nodiscard]] allocation_result_t alloc_bytes(size_t bytes,
                                                  uint8_t alignment_exponent,
                                                  size_t typehash) noexcept;

    [[nodiscard]] allocation_result_t remap_bytes(bytes_t mem,
                                                  size_t old_typehash,
                                                  size_t new_size,
                                                  size_t new_typehash) noexcept;

    allocation_status_t free_bytes(bytes_t mem, size_t typehash) noexcept;

    [[nodiscard]] allocation_status_t
    free_status(bytes_t mem, size_t typehash) const noexcept;

    allocation_status_t
    register_destruction_callback(destruction_callback_t callback,
                                  void* user_data) noexcept;

    ~block_allocator_t() noexcept;
    // cannot be copied
    block_allocator_t(const block_allocator_t& other) = delete;
    block_allocator_t& operator=(const block_allocator_t& other) = delete;
    // can be move constructed
    block_allocator_t(block_allocator_t&& other) noexcept;
    // but not move assigned
    block_allocator_t& operator=(block_allocator_t&& other) = delete;

  private:
    struct M
    {
        zl::opt<detail::abstract_heap_allocator_t&> parent;
        bytes_t mem;
        size_t last_freed_index;
        size_t blocks_free;
        const size_t blocksize;
        const size_t max_destruction_entries_per_block;
        size_t num_destruction_array_blocks;
        size_t current_destruction_array_index;
        size_t current_destruction_array_size;
    } m;

    static zl::res<block_allocator_t, AllocationStatusCode>
    make_inner(bytes_t memory,
               zl::opt<detail::abstract_heap_allocator_t&> parent,
               size_t blocksize) noexcept;

    static constexpr double reallocation_ratio = 1.5f;
    /// Remap our single allocation without moving it.
    [[nodiscard]] allocation_status_t remap() noexcept;

    struct destruction_callback_entry_t
    {
        destruction_callback_t callback;
        void* user_data;
    };

    struct destruction_callback_array_t
    {
        /// Index in the block allocator of the previously finished array of
        /// destruction callbacks
        size_t previous_index;
        // NOLINTNEXTLINE
        destruction_callback_entry_t entries[];
    };

    struct block_analysis_t
    {
        size_t byte_index;
        size_t block_index;
        uint8_t* first_byte;
    };

    static constexpr size_t minimum_blocksize =
        (sizeof(destruction_callback_entry_t) +
         sizeof(destruction_callback_entry_t));

  public:
    inline block_allocator_t(M&& members) noexcept : m(members)
    {
        m_type = enum_value;
    }

  private:
    void call_all_destruction_callbacks() const noexcept;
    size_t* get_location_for_typehash(uint8_t* blockhead,
                                      size_t allocsize) const noexcept;
    [[nodiscard]] zl::res<block_analysis_t, AllocationStatusCode>
    try_analyze_block(bytes_t mem, size_t typehash) const noexcept;
};
} // namespace allo

#ifdef ALLO_HEADER_ONLY
#include "allo/impl/block_allocator.h"
#endif
