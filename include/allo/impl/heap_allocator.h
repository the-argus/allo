#pragma once

#ifndef ALLO_HEADER_ONLY
#ifndef ALLO_OVERRIDE_IMPL_INCLUSION_GUARD
#error \
    "Attempt to include allo/impl header file but header-only mode is not enabled."
#endif
#endif

#include "allo/heap_allocator.h"

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
    free_node_t *next = nullptr;
};

ALLO_FUNC zl::res<heap_allocator_t, AllocationStatusCode>
heap_allocator_t::make_inner(const zl::slice<uint8_t> &memory,
                             zl::opt<DynamicHeapAllocatorRef> parent) noexcept
{
    void *head = memory.data();
    size_t space = memory.size();
    if (!std::align(alignof(free_node_t), sizeof(free_node_t), head, space)) {
        return AllocationStatusCode::OOM;
    }

    // NOTE: this effectively discards all bytes before the first 8 byte
    // boundary in the given memory.
    auto *memory_as_one_big_free_node = (free_node_t *)head;

    // write the size to the empty space
    *memory_as_one_big_free_node = {
        .size = memory.size(), // the whole memory is free
        .next = nullptr,
    };

    return zl::res<heap_allocator_t, AllocationStatusCode>{
        std::in_place,
        M{
            .parent = parent,
            .mem = memory,
            // 0 maximum contiguous bites == theoretically infinite
            // TODO: probably add some detection of the actual max alignment?
            // although it is a bit odd because a 9 byte heap which happens to
            // contain a 1048 byte boundary is 1048 byte aligned...
            .properties = allocator_properties_t(0, memory.size()),
            .num_nodes = 1,
            .num_callbacks = 0,
            .last_callback_node = nullptr,
            .free_list_head = memory_as_one_big_free_node,
        },
    };
}

ALLO_FUNC heap_allocator_t::heap_allocator_t(M &&members) noexcept
    : m(std::move(members))
{
    type = enum_value;
}

ALLO_FUNC heap_allocator_t::heap_allocator_t(heap_allocator_t &&other) noexcept
    : m(std::move(other.m))
{
    type = enum_value;
}

ALLO_FUNC heap_allocator_t::~heap_allocator_t() noexcept
{
    if (m.last_callback_node) {
        size_t num_in_last_node = m.num_callbacks % callbacks_per_node;
        // there is never 0 in the last node
        num_in_last_node =
            num_in_last_node == 0 ? callbacks_per_node : num_in_last_node;
        assert(num_in_last_node > 0 && num_in_last_node <= callbacks_per_node);
        for (auto &callback : zl::slice<destruction_callback_entry_t>(
                 m.last_callback_node->entries, 0, num_in_last_node)) {
            callback.callback(callback.user_data);
        }

        destruction_callback_node_t *iterator = m.last_callback_node->prev;
        while (iterator != nullptr) {
            for (auto &callback : iterator->entries) {
                callback.callback(callback.user_data);
            }
            iterator = iterator->prev;
        }
    }

    if (m.parent.has_value()) {
        m.parent.value().free_bytes(m.mem, 0);
    }
}

ALLO_FUNC allocation_status_t heap_allocator_t::register_destruction_callback(
    destruction_callback_t callback, void *user_data) noexcept
{
    const size_t callback_offset = m.num_callbacks % callbacks_per_node;
    // might be necessary to allocate space for some more destruction callbacks
    if (m.last_callback_node == nullptr || callback_offset == 0) {
        // allocate new destruction callback node
        auto res = alloc_bytes(sizeof(destruction_callback_node_t),
                               alignof(destruction_callback_node_t), 0);
        if (!res.okay())
            return res.err();
        auto *newnode = (destruction_callback_node_t *)res.release().data();
        newnode->prev = m.last_callback_node;
        m.last_callback_node = newnode;
    }

    m.last_callback_node->entries[callback_offset] = {
        .callback = callback,
        .user_data = user_data,
    };
    ++m.num_callbacks;
    return AllocationStatusCode::Okay;
}

ALLO_FUNC allocation_result_t
heap_allocator_t::realloc_bytes(zl::slice<uint8_t> mem, size_t old_typehash,
                                size_t new_size, size_t new_typehash) noexcept
{
}

ALLO_FUNC allocation_status_t
heap_allocator_t::free_bytes(zl::slice<uint8_t> mem, size_t typehash) noexcept
{
    auto res = free_common(mem, typehash);
    if (!res.okay())
        return res.err();
    auto *bk = res.release();

    auto *node = reinterpret_cast<free_node_t *>(bk);
    *node = free_node_t{.size = bk->size_actual, .next = m.free_list_head};
    m.free_list_head = node;

    return AllocationStatusCode::Okay;
}

ALLO_FUNC auto heap_allocator_t::free_common(zl::slice<uint8_t> mem,
                                             size_t typehash) const noexcept
    -> zl::res<allocation_bookkeeping_t *, AllocationStatusCode>
{

#ifndef NDEBUG
    void *head = mem.data();
    size_t dummyspace = sizeof(allocation_bookkeeping_t) * 2;
    assert(std::align(alignof(allocation_bookkeeping_t),
                      sizeof(allocation_bookkeeping_t), head,
                      dummyspace) == mem.data());
#endif
    auto *bk = static_cast<allocation_bookkeeping_t *>(head);
    while ((uint8_t *)(bk + 1) > mem.data()) {
        --bk;
    }
    if (bk->size_requested == mem.size()) {
        return AllocationStatusCode::MemoryInvalid;
    }
#ifndef ALLO_DISABLE_TYPEINFO
    // NOTE: this really should be a log message... you probably don't want
    // memory returning an error and therefore not being freed because of a
    // typing mistake. maybe would be different if free_bytes was always
    // nodiscard? using an assert to alert in debug mode
    assert(bk->typehash != typehash);
#endif
    return bk;
}

ALLO_FUNC allocation_result_t heap_allocator_t::alloc_bytes(
    size_t bytes, uint8_t alignment_exponent, size_t typehash) noexcept
{
    if (m.free_list_head == nullptr)
        return AllocationStatusCode::OOM;

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

    assert(m.free_list_head);
    free_node_t *prev = nullptr;
    free_node_t *iter = m.free_list_head;
    // NOTE: if alignment exponent is smaller than 3, this could be inaccurate
    size_t actual_size = bytes + sizeof(allocation_bookkeeping_t);
    while (iter != nullptr) {
        assert(iter->size <= m.mem.size());
        assert(((uint8_t *)iter >= m.mem.data() &&
                (uint8_t *)iter < m.mem.end().ptr()));
        if (iter->size >= actual_size) {
            size_t space = iter->size;
            void *block = iter;
            assert(std::align(alignof(allocation_bookkeeping_t),
                              sizeof(allocation_bookkeeping_t), block, space));
            assert(block == iter);
            assert(space > sizeof(allocation_bookkeeping_t));

            auto *bookkeeping =
                reinterpret_cast<allocation_bookkeeping_t *>(block);
            block = bookkeeping + 1;
            space -= sizeof(allocation_bookkeeping_t);

            if (!std::align(size_t(std::pow(2, actual_align_ex)), bytes, block,
                            space)) {
                continue;
            }
            assert(space >= bytes);
            space -= bytes;
            // space is now the number of bytes left in this block...
            void *remaining = ((uint8_t *)block) + bytes;
            bool is_space_remaining = std::align(
                alignof(free_node_t), sizeof(free_node_t), remaining, space);
            if (is_space_remaining) {
                assert(remaining != nullptr);
                // able to fit a free node in here, use that to shrink the size
                auto *newnode = reinterpret_cast<free_node_t *>(remaining);
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

            const size_t diff = reinterpret_cast<uint8_t *>(bookkeeping) -
                                reinterpret_cast<uint8_t *>(iter);
            assert(diff < alignof(allocation_bookkeeping_t));
            assert((void *)bookkeeping == iter);
            *bookkeeping = {
                .size_requested = bytes,
                // NOTE: reading from next right before we assign over it...
                .size_actual =
                    is_space_remaining
                        ? size_t(((uint8_t *)remaining) - ((uint8_t *)iter))
                        : iter->size,
#ifndef ALLO_DISABLE_TYPEINFO
                .typehash = typehash,
#endif
            };

            return zl::raw_slice(*reinterpret_cast<uint8_t *>(block), bytes);
        }
        prev = iter;
        iter = iter->next;
    }
    // TODO: request more space from parent if owning
    return AllocationStatusCode::OOM;
}

ALLO_FUNC allocation_status_t heap_allocator_t::free_status(
    zl::slice<uint8_t> mem, size_t typehash) const noexcept
{
    return free_common(mem, typehash).err();
}

} // namespace allo
