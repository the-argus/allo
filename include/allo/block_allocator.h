#pragma once

#include "allo/detail/abstracts.h"
#include "allo/detail/destruction_callback.h"
#include "allo/structures/any_allocator.h"
#include "allo/structures/segmented_stack.h"

namespace allo {

class block_allocator_t : public detail::abstract_heap_allocator_t
{
  private:
    struct M
    {
        bytes_t memory;
        size_t blocks_free;
        size_t total_blocks;
        const size_t blocksize;
        detail::destruction_callback_entry_list_node_t* last_callback_array =
            nullptr;
        size_t last_callback_array_size;
        segmented_stack_t<bytes_t>* blocks = nullptr;
        // head of free list
        void* last_freed;
        any_allocator_t parent;
    } m;

  public:
    static constexpr detail::AllocatorType enum_value =
        detail::AllocatorType::BlockAllocator;

    /// Create a block allocator which will attempt to free its memory when it
    /// is destroyed, and will try to remap the memory should it run out of
    /// space.
    inline static block_allocator_t
    make_owning(bytes_t memory, detail::abstract_heap_allocator_t& parent,
                size_t blocksize) noexcept
    {
        return make_inner(memory, parent, blocksize);
    }

    /// Create a block allocator which allocates into a given block of memory.
    inline static block_allocator_t make(bytes_t memory,
                                         size_t blocksize) noexcept
    {
        return make_inner(memory, {}, blocksize);
    }

    /// Create a block allocator which allocates into a given block of memory,
    /// and is capable of allocating new blocks when the first one runs out
    inline static block_allocator_t make(bytes_t memory,
                                         detail::abstract_allocator_t& parent,
                                         size_t blocksize) noexcept
    {
        return make_inner(memory, parent, blocksize);
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
    static block_allocator_t make_inner(bytes_t memory, any_allocator_t parent,
                                        size_t blocksize) noexcept;

    static constexpr size_t minimum_blocksize = 32;
    static_assert(detail::bytes_needed_for_destruction_callback_v<1> + 8 ==
                  minimum_blocksize);

  public:
    inline block_allocator_t(M&& members) noexcept : m(members)
    {
        m_type = enum_value;
    }

  private:
#ifndef ALLO_DISABLE_TYPEINFO
    size_t* get_location_for_typehash(uint8_t* blockhead,
                                      size_t allocsize) const noexcept;
#endif
    [[nodiscard]] size_t max_destruction_entries_per_block() const noexcept;
    static constexpr double growth_percentage = 0.5f;
    /// Multiply total blocks by 1 + growth percentage
    [[nodiscard]] allocation_status_t grow() noexcept;

#ifndef NDEBUG
    [[nodiscard]] bool contains(bytes_t) const noexcept;
#endif
};
} // namespace allo

#ifdef ALLO_HEADER_ONLY
#include "allo/impl/block_allocator.h"
#endif
