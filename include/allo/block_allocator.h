#pragma once

#include "allo/allocator_interfaces.h"

namespace allo {

class block_allocator_t : public detail::allocator_t,
                          public detail::freer_t,
                          public detail::reallocator_t,
                          public detail::destruction_callback_provider_t,
                          private detail::dynamic_allocator_base_t
{
  public:
    static constexpr detail::AllocatorType enum_value =
        detail::AllocatorType::BlockAllocator;

    static zl::res<block_allocator_t, AllocationStatusCode>
    make(const zl::slice<uint8_t> &memory,
         allocator_with<IRealloc, IFree> &parent, size_t blocksize,
         uint8_t alignment_exponent) noexcept;

    [[nodiscard]] allocation_result_t alloc_bytes(size_t bytes,
                                                  uint8_t alignment_exponent,
                                                  size_t typehash) noexcept;

    [[nodiscard]] allocation_result_t
    realloc_bytes(zl::slice<uint8_t> mem, size_t old_typehash, size_t new_size,
                  size_t new_typehash) noexcept;

    allocation_status_t free_bytes(zl::slice<uint8_t> mem,
                                   size_t typehash) noexcept;

    [[nodiscard]] allocation_status_t
    free_status(zl::slice<uint8_t> mem, size_t typehash) const noexcept;

    [[nodiscard]] inline constexpr const allocator_properties_t &
    properties() const noexcept
    {
        return m.properties;
    }

    allocation_status_t
    register_destruction_callback(destruction_callback_t callback,
                                  void *user_data) noexcept;

    ~block_allocator_t() noexcept;
    // cannot be copied
    block_allocator_t(const block_allocator_t &other) = delete;
    block_allocator_t &operator=(const block_allocator_t &other) = delete;
    // can be move constructed
    block_allocator_t(block_allocator_t &&other) noexcept;
    // but not move assigned
    block_allocator_t &operator=(block_allocator_t &&other) = delete;

  private:
    struct M
    {
        allocator_with<IRealloc, IFree> &parent;
        zl::slice<uint8_t> mem;
        allocator_properties_t properties;
        size_t last_freed_index;
        size_t blocks_free;
        size_t blocksize;
        size_t max_destruction_entries_per_block;
        size_t num_destruction_array_blocks;
        size_t current_destruction_array_index;
        size_t current_destruction_array_size;
        // whether to free memory in destructor
        bool owning = true;
    } m;

    struct destruction_callback_entry_t
    {
        destruction_callback_t callback;
        void *user_data;
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
        uint8_t *first_byte;
    };

    static constexpr size_t minimum_blocksize =
        (sizeof(destruction_callback_entry_t) +
         sizeof(destruction_callback_entry_t));

    inline block_allocator_t(M &&members) noexcept : m(members)
    {
        type = enum_value;
    }

    void call_all_destruction_callbacks() const noexcept;
    size_t *get_location_for_typehash(uint8_t *blockhead,
                                      size_t allocsize) const noexcept;
    [[nodiscard]] zl::res<block_analysis_t, AllocationStatusCode>
    try_analyze_block(zl::slice<uint8_t> mem, size_t typehash) const noexcept;
};
} // namespace allo

#ifdef ALLO_HEADER_ONLY
#include "allo/impl/block_allocator.h"
#endif
