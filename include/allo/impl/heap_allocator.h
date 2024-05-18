#pragma once

#include <cmath>
#ifndef ALLO_HEADER_ONLY
#ifndef ALLO_OVERRIDE_IMPL_INCLUSION_GUARD
#error \
    "Attempt to include allo/impl header file but header-only mode is not enabled."
#endif
#endif

#include "allo/heap_allocator.h"
#include <memory>

#ifdef ALLO_HEADER_ONLY
#ifndef ALLO_FUNC
#define ALLO_FUNC inline
#endif
#else
#define ALLO_FUNC
#endif

namespace allo {

struct heap_allocator_t::free_node_t
{
    size_t size;
    free_node_t* next = nullptr;
};

ALLO_FUNC heap_allocator_t heap_allocator_t::make_inner(
    const bytes_t& memory, any_allocator_t parent) noexcept
{
    void* head = memory.data();
    size_t space = memory.size();
    if (!std::align(alignof(free_node_t), sizeof(free_node_t), head, space)) {
        std::abort();
    }

    // NOTE: this effectively discards all bytes before the first 8 byte
    // boundary in the given memory.
    auto* memory_as_one_big_free_node = (free_node_t*)head;

    // write the size to the empty space
    *memory_as_one_big_free_node = {
        .size = memory.size(), // the whole memory is free
        .next = nullptr,
    };

    return M{
        .memory = memory,
        .current_memory_original_size = memory.size(),
        .free_list_head = memory_as_one_big_free_node,
        .parent = parent,
    };
}

ALLO_FUNC heap_allocator_t::heap_allocator_t(M&& members) noexcept : m(members)
{
    m_type = enum_value;
}

ALLO_FUNC heap_allocator_t::heap_allocator_t(heap_allocator_t&& other) noexcept
    : m(other.m)
{
    m_type = enum_value;
    other.m.parent = {};
    other.m.last_callback_node = nullptr;
}

ALLO_FUNC heap_allocator_t::~heap_allocator_t() noexcept
{
    detail::call_all_destruction_callback_arrays<destruction_callback_node_t>(
        m.last_callback_node, destruction_callback_node_t::num_entries,
        m.last_callback_array_size);

    if (!m.parent.is_heap())
        return;

    if (m.blocks) {
        while (auto iter = m.blocks->end()) {
            m.parent.get_heap_unchecked().free_bytes(iter.value(), 0);
            m.blocks->pop();
        }
        m.blocks->~segmented_stack_t<bytes_t>();
        free_one(m.parent.get_heap_unchecked(), *m.blocks);
    } else {
        m.parent.get_heap_unchecked().free_bytes(m.memory, 0);
    }
}

ALLO_FUNC allocation_status_t heap_allocator_t::register_destruction_callback(
    destruction_callback_t callback, void* user_data) noexcept
{
    if (!callback) {
        assert(callback);
        return AllocationStatusCode::InvalidArgument;
    }
    // might be necessary to allocate space for some more destruction callbacks
    if (m.last_callback_node == nullptr ||
        m.last_callback_array_size ==
            destruction_callback_node_t::num_entries) {
        // allocate new destruction callback node
        auto res = alloc_bytes(sizeof(destruction_callback_node_t),
                               alignof(destruction_callback_node_t), 0);
        if (!res.okay())
            return res.err();
        auto* newnode = (destruction_callback_node_t*)res.release().data();
        newnode->prev = m.last_callback_node;
        m.last_callback_node = newnode;
    }

    assert(m.last_callback_array_size <
           destruction_callback_node_t::num_entries);

    m.last_callback_node->entries[m.last_callback_array_size] = {
        .callback = callback,
        .user_data = user_data,
    };

    ++m.last_callback_array_size;

    return AllocationStatusCode::Okay;
}

ALLO_FUNC allocation_result_t
heap_allocator_t::remap_bytes(bytes_t mem, size_t old_typehash, size_t new_size,
                              size_t new_typehash) noexcept
{
    // not possible to remap
    // TODO: remapping with this kind of allocator? maybe?
    if (mem.size() < new_size) {
        return AllocationStatusCode::OOM;
    }

    if (old_typehash != new_typehash) {
        // no changing types with heap allocator
        assert(old_typehash == new_typehash);
        return AllocationStatusCode::InvalidArgument;
    }

    return bytes_t(mem, 0, new_size);
}

ALLO_FUNC allocation_status_t
heap_allocator_t::free_bytes(bytes_t mem, size_t typehash) noexcept
{
    auto res = free_common(mem, typehash);
    if (!res.okay())
        return res.err();
    auto* bk = res.release();

    auto* node = reinterpret_cast<free_node_t*>(bk);
    *node = free_node_t{.size = bk->size_actual, .next = m.free_list_head};
    m.free_list_head = node;

    return AllocationStatusCode::Okay;
}

ALLO_FUNC auto heap_allocator_t::free_common(bytes_t mem,
                                             size_t typehash) const noexcept
    -> zl::res<allocation_bookkeeping_t*, AllocationStatusCode>
{

    void* head = mem.data();
#ifndef NDEBUG
    {
        size_t debug_space = sizeof(allocation_bookkeeping_t) * 2;
        void* debug_head = head;
        assert(std::align(alignof(allocation_bookkeeping_t),
                          sizeof(allocation_bookkeeping_t), debug_head,
                          debug_space) == mem.data());
    }
#endif
    auto* bk = static_cast<allocation_bookkeeping_t*>(head);
    while ((uint8_t*)(bk + 1) > mem.data()) {
        --bk;
    }
    // make sure magic matches. if it doesnt, treat it as a pointer to the
    // actual allocation_bookkeeping_t
    if (bk->magic != allocation_bookkeeping_t::static_magic) {
        bk = reinterpret_cast<allocation_bookkeeping_t*>(bk->magic); // NOLINT
        assert(contains(
            zl::raw_slice(*(uint8_t*)bk, sizeof(allocation_bookkeeping_t))));
    }
    if (bk->size_requested == mem.size()) {
        return AllocationStatusCode::MemoryInvalid;
    }
#ifndef ALLO_DISABLE_TYPEINFO
    // NOTE: this really should be a log message
    // TODO: return error if type doesnt match on a free? idk if this would be a
    // good idea
    assert(bk->typehash == typehash);
#endif
    return bk;
}

/// NOTE: this function is the same as in block_allocator_t
#ifndef NDEBUG
ALLO_FUNC bool heap_allocator_t::contains(bytes_t bytes) const noexcept
{
    if (m.blocks) {
        // assert that exactly one of the items in the blocks stack is a
        // block of memory which contains the next_to_last_freed item
        bool contains = false;
        // TODO: implement std iterators so we can do std::find
        m.blocks->for_each([&contains, &bytes](const bytes_t& block) {
            if (zl::memcontains(block, bytes)) {
                assert(contains == false);
                contains = true;
            }
        });
        return contains;
    } else {
        return zl::memcontains(m.memory, bytes);
    }
}
#endif

ALLO_FUNC allocation_result_t heap_allocator_t::alloc_bytes(
    size_t bytes, uint8_t alignment_exponent, size_t typehash) noexcept
{
    if (m.free_list_head == nullptr)
        return AllocationStatusCode::OOM;
    auto res = alloc_bytes_inner(bytes, alignment_exponent, typehash,
                                 m.free_list_head);
    if (!res.success) {
        allocation_status_t status = try_make_space_for_at_least(
            res.actual_needed_size, res.last_searched);
        if (!status.okay())
            return status.err();
        auto second_attempt = alloc_bytes_inner(bytes, alignment_exponent,
                                                typehash, res.last_searched);
        assert(second_attempt.success);
        return bytes_t(second_attempt.success.value());
    }
    return bytes_t(res.success.value());
}

ALLO_FUNC auto heap_allocator_t::alloc_bytes_inner(
    size_t bytes, uint8_t alignment_exponent, size_t typehash,
    free_node_t* last_searched_node) noexcept -> inner_allocation_attempt_t
{
    assert(last_searched_node);
    // refuse to align to less than 8 bytes
    const size_t actual_align_ex =
        alignment_exponent < 3 ? 3 : alignment_exponent;
    static_assert(
        detail::nearest_alignment_exponent(alignof(allocation_bookkeeping_t)) ==
            3,
        "The reason heap allocator aligns to 2^3 is because thats the expected "
        "alignment of its linked list nodes, but that is not correct");
    static_assert(alignof(allocation_bookkeeping_t) == alignof(free_node_t),
                  "heap allocator relies on free nodes and allocation nodes "
                  "being able to have the same allocation");

    free_node_t* prev = nullptr;
    free_node_t* iter = last_searched_node;
    // NOTE: if alignment exponent is smaller than 3, this could be inaccurate
    size_t actual_size = bytes +
                         static_cast<size_t>(std::pow(2, actual_align_ex)) +
                         sizeof(allocation_bookkeeping_t);
    while (iter != nullptr) {
        assert(contains(zl::raw_slice(*(uint8_t*)iter, sizeof(free_node_t))));
        if (iter->size >= actual_size) {
            // make sure everything is aligned and has space
#ifndef NDEBUG
            {
                size_t space = iter->size;
                void* block = iter;
                assert(std::align(alignof(allocation_bookkeeping_t),
                                  sizeof(allocation_bookkeeping_t), block,
                                  space));
                assert(block == iter);
                assert(space > sizeof(allocation_bookkeeping_t));
            }
#endif
            size_t space = iter->size;
            void* block = iter;

            auto* bookkeeping =
                reinterpret_cast<allocation_bookkeeping_t*>(block);
            block = bookkeeping + 1;
            space -= sizeof(allocation_bookkeeping_t);

            // keep track of where we were originally planning to allocate
            // before alignment, so we can see if we moved when aligning
            void* original_block = block;

            if (!std::align(size_t(std::pow(2, actual_align_ex)), bytes, block,
                            space)) {
                continue;
            }
            if (block != original_block) {
                // make sure we moved by at least 8 bytes. doesnt make sense to
                // move any less since we were already 8-byte aligned
                assert((uint8_t*)block - (uint8_t*)original_block >
                       sizeof(void*));
                // go back before the block, place down a pointer to the actual
                // allocation_bookkeeping_t.
                *(((allocation_bookkeeping_t**)block) - 1) = bookkeeping;
            }
            assert(space >= bytes);
            space -= bytes;
            // space is now the number of bytes left in this block...
            void* remaining = ((uint8_t*)block) + bytes;
            bool is_space_remaining = std::align(
                alignof(free_node_t), sizeof(free_node_t), remaining, space);
            if (is_space_remaining) {
                assert(remaining != nullptr);
                // able to fit a free node in here, use that to shrink the size
                auto* newnode = reinterpret_cast<free_node_t*>(remaining);
                *newnode = {.size = space, .next = iter->next};

                // make the previous item point to our new node. behavior has to
                // be different if we're at the beginning of the list, though
                if (prev) {
                    prev->next = newnode;
                } else {
                    m.free_list_head = newnode;
                }
            } else {
                // can't fit free node, so we must be taking up the whole block
                // remove the free node from this, before we write to anything
                // (read the "next" pointer first)
                if (prev) {
                    // remove ourselves from the list
                    prev->next = iter->next;
                } else {
                    // first node, so just set the first node to be the
                    // now-second node
                    m.free_list_head = iter->next;
                }
            }

            const size_t diff = reinterpret_cast<uint8_t*>(bookkeeping) -
                                reinterpret_cast<uint8_t*>(iter);
            assert(diff < alignof(allocation_bookkeeping_t));
            assert((void*)bookkeeping == iter);
            *bookkeeping = {
                .size_requested = bytes,
                // NOTE: reading from next right before we assign over it...
                .size_actual =
                    is_space_remaining
                        ? size_t(((uint8_t*)remaining) - ((uint8_t*)iter))
                        : iter->size,
#ifndef ALLO_DISABLE_TYPEINFO
                .typehash = typehash,
#endif
            };

            return inner_allocation_attempt_t{
                .success =
                    zl::raw_slice(*reinterpret_cast<uint8_t*>(block), bytes)};
        }
        prev = iter;
        iter = iter->next;
    }
    return inner_allocation_attempt_t{
        .last_searched = prev,
        .actual_needed_size = actual_size,
    };
}

ALLO_FUNC size_t heap_allocator_t::round_up_to_valid_buffersize(
    size_t needed_bytes, size_t original_size) noexcept
{
    // NOTE: the fact that we always grow the buffer by two is SUPER
    // hardcoded here since we use log2. you'll have to use some fancy
    // log rules to get log base 1.5 or some other multiplier
    return static_cast<size_t>(std::round(
        std::pow(2.0,
                 std::floor(std::log2(static_cast<double>(needed_bytes) /
                                      static_cast<double>(original_size))) +
                     1) *
        static_cast<double>(original_size)));
}

ALLO_FUNC allocation_status_t heap_allocator_t::try_make_space_for_at_least(
    size_t bytes, free_node_t* newmem_insert_location) noexcept
{
    if (m.parent.is_null())
        return AllocationStatusCode::OOM;

    bytes_t oldmem = m.memory;
    constexpr auto minbytes = sizeof(free_node_t) + alignof(free_node_t);
    const size_t actual_bytes = bytes < minbytes ? minbytes : bytes;

    // function which inserts a free node into memory and into the free list
    auto format_new_memory = [newmem_insert_location](void* _head,
                                                      size_t _space) {
        // update new memory to be a new free node
        void* head = _head;
        size_t space = _space;
        void* const result =
            std::align(alignof(free_node_t), sizeof(free_node_t), head, space);
        assert(result);

        auto* new_free_node = reinterpret_cast<free_node_t*>(result);
        *new_free_node = free_node_t{
            .size = space,
            .next = newmem_insert_location->next,
        };
        newmem_insert_location->next = new_free_node;
    };

    if (m.parent.is_heap()) {
        const size_t new_size_remapped = round_up_to_valid_buffersize(
            actual_bytes + m.memory.size(), m.current_memory_original_size);
        assert(new_size_remapped >= m.memory.size() + actual_bytes);
        auto res = m.parent.get_heap_unchecked().remap_bytes(
            m.memory, 0, new_size_remapped, 0);
        if (res.okay()) {
            if (m.blocks) {
                assert(m.blocks->end_unchecked() == m.memory);
                m.blocks->pop();
                m.memory = res.release();
                const auto pushres = m.blocks->try_push(m.memory);
                assert(pushres.okay());
            } else {
                m.memory = res.release();
            }

            format_new_memory(oldmem.end().ptr(),
                              m.memory.size() - oldmem.size());
            return AllocationStatusCode::Okay;
        }
    }

    auto& parent = m.parent.cast_to_basic();

    // either we're not a heap allocator or remapping failed
    // first, create the blocks data structure so we can store the new block
    if (!m.blocks) {
        auto blocks_allocation_res =
            alloc_one<segmented_stack_t<bytes_t>>(parent);
        if (!blocks_allocation_res.okay()) [[unlikely]]
            return blocks_allocation_res.err();

        auto blocks_res = m.parent.is_heap()
                              ? segmented_stack_t<bytes_t>::make_owning(
                                    m.parent.get_heap_unchecked(), 2)
                              : segmented_stack_t<bytes_t>::make(parent, 2);
        if (!blocks_res.okay()) [[unlikely]] {
            if (m.parent.is_heap()) {
                free_one(m.parent.get_heap_unchecked(),
                         blocks_allocation_res.release());
            }
            return blocks_res.err();
        }
        // explicitly move the created segment from the stack to the
        // allocated blocks
        // TODO: use some make_into here to avoid this move
        m.blocks = &blocks_allocation_res.release();
        new (m.blocks) segmented_stack_t<bytes_t>(blocks_res.release());
        auto pushres = m.blocks->try_push(m.memory);
        assert(pushres.okay());
    }

    assert(m.blocks->end_unchecked() == m.memory);
    if (const auto pushres = m.blocks->try_push(m.memory); !pushres.okay())
        return pushres.err();

    // make actual allocation for the new buffer
    auto res =
        parent.alloc_bytes(round_up_to_valid_buffersize(
                               actual_bytes, m.current_memory_original_size),
                           alignof(free_node_t), 0);
    if (!res.okay()) [[unlikely]] {
        m.blocks->pop();
        return res.err();
    }

    m.memory = res.release_ref();
    m.blocks->end_unchecked() = m.memory;
    format_new_memory(m.memory.data(), m.memory.size());
    // new current memory, so new original size
    m.current_memory_original_size = m.memory.size();
    return AllocationStatusCode::Okay;
}

ALLO_FUNC allocation_status_t
heap_allocator_t::free_status(bytes_t mem, size_t typehash) const noexcept
{
    return free_common(mem, typehash).err();
}

} // namespace allo
