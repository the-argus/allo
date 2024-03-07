#pragma once

#include "allo/abstracts.h"

namespace allo {

class heap_allocator_t : private detail::dynamic_allocator_base_t
{
  public:
    struct destruction_callback_entry_t
    {
        destruction_callback_t callback;
        void *user_data;
    };
    // destruction callbacks are grouped in three since thats how many we
    // can fit in a cache line
    struct destruction_callback_node_t
    {
        destruction_callback_node_t *prev;
        std::array<destruction_callback_entry_t, 3> entries;
    };
    struct free_node_t;
    /// Stored before every allocation
    struct allocation_bookkeeping_t
    {
        size_t size_requested;
        size_t size_actual;
#ifndef ALLO_DISABLE_TYPEINFO
        size_t typehash;
#endif
    };

  private:
    static constexpr size_t callbacks_per_node =
        (sizeof(destruction_callback_node_t::entries) /
         sizeof(destruction_callback_entry_t));

    struct M
    {
        zl::opt<DynamicHeapAllocatorRef> parent;
        zl::slice<uint8_t> mem;
        allocator_properties_t properties;
        size_t num_nodes; // also == number of entries in free list data
        size_t num_callbacks = 0;
        destruction_callback_node_t *last_callback_node = nullptr;
        free_node_t *free_list_head;
    } m;

    static zl::res<heap_allocator_t, AllocationStatusCode>
    make_inner(const zl::slice<uint8_t> &memory,
               const zl::opt<DynamicHeapAllocatorRef> &parent) noexcept;

  public:
    static constexpr detail::AllocatorType enum_value =
        detail::AllocatorType::HeapAllocator;

    template <typename Allocator>
    inline static zl::res<heap_allocator_t, AllocationStatusCode>
    make_owned(zl::slice<uint8_t> memory, Allocator &parent) noexcept
    {
        return make_inner(memory, DynamicHeapAllocatorRef(parent));
    }

    inline static zl::res<heap_allocator_t, AllocationStatusCode>
    make(zl::slice<uint8_t> memory) noexcept
    {
        return make_inner(memory, {});
    }

    [[nodiscard]] allocation_result_t
    realloc_bytes(zl::slice<uint8_t> mem, size_t old_typehash, size_t new_size,
                  size_t new_typehash) noexcept;

    allocation_status_t free_bytes(zl::slice<uint8_t> mem,
                                   size_t typehash) noexcept;

    [[nodiscard]] allocation_result_t alloc_bytes(size_t bytes,
                                                  uint8_t alignment_exponent,
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

    ~heap_allocator_t() noexcept;
    // cannot be copied
    heap_allocator_t(const heap_allocator_t &other) = delete;
    heap_allocator_t &operator=(const heap_allocator_t &other) = delete;
    // can be move constructed
    heap_allocator_t(heap_allocator_t &&other) noexcept;
    // but not move assigned
    heap_allocator_t &operator=(heap_allocator_t &&other) = delete;

    heap_allocator_t(M &&members) noexcept;

  private:
    [[nodiscard]] zl::res<allocation_bookkeeping_t *, AllocationStatusCode>
    free_common(zl::slice<uint8_t> mem, size_t typehash) const noexcept;
};
} // namespace allo

#ifdef ALLO_HEADER_ONLY
#include "allo/impl/heap_allocator.h"
#endif
