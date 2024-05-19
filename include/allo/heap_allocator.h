#pragma once

#include "allo/detail/abstracts.h"
#include "allo/detail/destruction_callback.h"
#include "allo/structures/any_allocator.h"
#include "allo/structures/segmented_stack.h"

namespace allo {

class heap_allocator_t : public detail::abstract_heap_allocator_t
{
  public:
    using destruction_callback_node_t =
        detail::destruction_callback_entry_list_node_cacheline_t;
    struct free_node_t;
    /// Stored before every allocation
    struct allocation_bookkeeping_t
    {
        size_t size_requested;
        size_t size_actual;
#ifndef ALLO_DISABLE_TYPEINFO
        size_t typehash;
#endif
        static constexpr size_t static_magic = 0xDEADBEEF;
        // NOTE: this is here so that we can check if the bookkeeping is stored
        // right behind the allocation. if the bytes behind an allocation dont
        // match static_magic, then we interpret them as a pointer to the
        // allocation_bookkeeping_t.
        size_t magic = static_magic;
    };

  private:
    struct M
    {
        bytes_t memory;
        size_t current_memory_original_size;
        destruction_callback_node_t* last_callback_node;
        size_t last_callback_array_size;
        free_node_t* free_list_head;
        segmented_stack_t<bytes_t>* blocks;
        any_allocator_t parent;
    } m;

    [[nodiscard]] static heap_allocator_t
    make_inner(const bytes_t& memory, any_allocator_t parent) noexcept;

  public:
    static constexpr detail::AllocatorType enum_value =
        detail::AllocatorType::HeapAllocator;

    // NOTE: unlike other allocators, it is possible to pass invalid arguments
    // to the make function. If the given memory is too small, std::abort() will
    // be called. It must be at least be able to fit one 16 byte, 8-byte-aligned
    // struct. 32 bytes is guaranteed to be enough space.

    [[nodiscard]] inline static heap_allocator_t
    make_owning(bytes_t memory,
                detail::abstract_heap_allocator_t& parent) noexcept
    {
        return make_inner(memory, parent);
    }

    [[nodiscard]] inline static heap_allocator_t
    make(bytes_t memory, detail::abstract_allocator_t& parent) noexcept
    {
        return make_inner(memory, parent);
    }

    [[nodiscard]] inline static heap_allocator_t make(bytes_t memory) noexcept
    {
        return make_inner(memory, {});
    }

    [[nodiscard]] allocation_result_t remap_bytes(bytes_t mem,
                                                  size_t old_typehash,
                                                  size_t new_size,
                                                  size_t new_typehash) noexcept;

    allocation_status_t free_bytes(bytes_t mem, size_t typehash) noexcept;

    [[nodiscard]] allocation_result_t alloc_bytes(size_t bytes,
                                                  uint8_t alignment_exponent,
                                                  size_t typehash) noexcept;

    [[nodiscard]] allocation_status_t
    free_status(bytes_t mem, size_t typehash) const noexcept;

    allocation_status_t
    register_destruction_callback(destruction_callback_t callback,
                                  void* user_data) noexcept;

    ~heap_allocator_t() noexcept;
    // cannot be copied
    heap_allocator_t(const heap_allocator_t& other) = delete;
    heap_allocator_t& operator=(const heap_allocator_t& other) = delete;
    // can be move constructed
    heap_allocator_t(heap_allocator_t&& other) noexcept;
    // but not move assigned
    heap_allocator_t& operator=(heap_allocator_t&& other) = delete;

    heap_allocator_t(M&& members) noexcept;

  private:
    struct inner_allocation_attempt_t
    {
        free_node_t* last_searched;
        size_t actual_needed_size;
        zl::opt<bytes_t> success;
    };

    [[nodiscard]] static size_t
    round_up_to_valid_buffersize(size_t needed_bytes,
                                 size_t original_size) noexcept;

    [[nodiscard]] inner_allocation_attempt_t
    alloc_bytes_inner(size_t bytes, uint8_t alignment_exponent, size_t typehash,
                      free_node_t* last_searched_node) noexcept;

    [[nodiscard]] allocation_status_t
    try_make_space_for_at_least(size_t bytes,
                                free_node_t* newmem_insert_location) noexcept;

    [[nodiscard]] zl::res<allocation_bookkeeping_t*, AllocationStatusCode>
    free_common(bytes_t mem, size_t typehash) const noexcept;
#ifndef NDEBUG
    [[nodiscard]] bool contains(bytes_t) const noexcept;
#endif
};
} // namespace allo

#ifdef ALLO_HEADER_ONLY
#include "allo/impl/heap_allocator.h"
#endif
