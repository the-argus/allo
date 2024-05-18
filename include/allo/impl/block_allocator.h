#pragma once

#ifndef ALLO_HEADER_ONLY
#ifndef ALLO_OVERRIDE_IMPL_INCLUSION_GUARD
#error \
    "Attempt to include allo/impl header file but header-only mode is not enabled."
#endif
#endif

#include "allo/block_allocator.h"
#include "allo/detail/alignment.h"
#include <cmath>
#include <memory>
#include <ziglike/stdmem.h>

#ifdef ALLO_HEADER_ONLY
#ifndef ALLO_FUNC
#define ALLO_FUNC inline
#endif
#else
#define ALLO_FUNC
#endif

namespace allo {

ALLO_FUNC block_allocator_t::~block_allocator_t() noexcept
{
    detail::call_all_destruction_callback_arrays(
        m.last_callback_array, max_destruction_entries_per_block(),
        m.last_callback_array_size);

    if (m.parent.is_heap()) {
        // TODO: make this traverse m.blocks
        m.parent.get_heap_unchecked().free_bytes(m.memory, 0);
    }
}

ALLO_FUNC
block_allocator_t::block_allocator_t(block_allocator_t&& other) noexcept
    : m(other.m)
{
    m_type = other.m_type;
#ifndef NDEBUG
    // make other now return OOM whenever you try to allocate from it
    other.m.parent = {};
    other.m.blocks_free = 0;
#endif
}

ALLO_FUNC block_allocator_t block_allocator_t::make_inner(
    bytes_t memory, any_allocator_t parent, size_t blocksize) noexcept
{
    // blocksize must be at least 24 bytes, to fit
    size_t actual_blocksize =
        (blocksize < minimum_blocksize) ? minimum_blocksize : blocksize;

#ifndef NDEBUG
    if (parent.is_heap()) {
        assert(parent.get_heap_unchecked().free_status(memory, 0).okay());
    }
#endif

    auto num_blocks =
        static_cast<size_t>(std::floor(static_cast<double>(memory.size()) /
                                       static_cast<double>(actual_blocksize)));
    assert(num_blocks < memory.size() + 1);

    // its not really valid to create a block allocator with no intial blocks,
    // although it wont return an error until you go to allocate with it
    assert(num_blocks > 0);

    for (size_t i = 0; i < num_blocks; ++i) {
        uint8_t* head = memory.data() + (i * actual_blocksize);
        assert(detail::nearest_alignment_exponent((size_t)head) >= 3);
        *reinterpret_cast<void**>(head) = head + actual_blocksize;
    }

    return M{
        .memory = memory,
        // all are free at start
        .blocks_free = num_blocks,
        .total_blocks = num_blocks,
        .blocksize = actual_blocksize,
        .last_freed = memory.data(),
        // parent allocator. if null, we don't own the allocation
        .parent = parent,
    };
}

ALLO_FUNC size_t
block_allocator_t::max_destruction_entries_per_block() const noexcept
{
    size_t max_destruction_entries_per_block =
        (m.blocksize - sizeof(detail::destruction_callback_entry_list_node_t)) /
        sizeof(detail::destruction_callback_entry_t);
    assert(max_destruction_entries_per_block >= 1);
    return max_destruction_entries_per_block;
}

ALLO_FUNC allocation_status_t block_allocator_t::grow() noexcept
{
    if (m.parent.is_null())
        return AllocationStatusCode::OOM;

    const auto additional_blocks_needed = static_cast<size_t>(
        std::ceil(static_cast<float>(m.total_blocks) * growth_percentage));
    const size_t additional_bytes_needed =
        additional_blocks_needed * m.blocksize;

    bytes_t oldmem = m.memory;

    // first just try to remap
    if (m.parent.is_heap()) {
        auto res = m.parent.get_heap_unchecked().remap_bytes(
            m.memory, 0, m.memory.size() + additional_bytes_needed, 0);
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
            m.total_blocks += additional_blocks_needed;

            // initialize each block to point to the next one
            for (size_t i = 0; i < additional_blocks_needed; ++i) {
                uint8_t* head = oldmem.end().ptr() + (i * m.blocksize);
                assert(zl::memcontains_one(m.memory, head));
                *reinterpret_cast<void**>(head) = head + m.blocksize;
            }

            // we would need to update free list end to point to the new area
            // otherwise
            assert(m.blocks_free == 0);

            m.blocks_free += additional_blocks_needed;

            m.last_freed = oldmem.end().ptr();

            return AllocationStatusCode::Okay;
        }
    }

    auto& parent = m.parent.cast_to_basic();

    // we need a structure to keep track of our allocations
    if (!m.blocks) {
        auto res = alloc_one<segmented_stack_t<bytes_t>>(parent);
        if (!res.okay()) [[unlikely]]
            return AllocationStatusCode::OOM;
        m.blocks = &res.release();
        auto blocks = m.parent.is_heap()
                          ? segmented_stack_t<bytes_t>::make_owning(
                                m.parent.get_heap_unchecked(), 2)
                          : segmented_stack_t<bytes_t>::make(parent, 2);
        if (!blocks.okay()) [[unlikely]]
            return blocks.err();
        // explicitly move the created segment from the stack to the
        // allocated blocks
        // TODO: use some make_into here to avoid this move
        new (m.blocks) segmented_stack_t<bytes_t>(blocks.release());
        auto pushres = m.blocks->try_push(m.memory);
        assert(pushres.okay());
    }
    assert(m.blocks);
    assert(m.blocks->end());
    assert(m.blocks->end_unchecked() == m.memory);

    // okay now we are guaranteed to have blocks structure and we need another
    // item in it, remapping has failed
    // start by pushing to the stack to be sure it can fit another entry
    if (const auto pushres = m.blocks->try_push(m.memory); !pushres.okay())
        return pushres.err();

    auto res =
        parent.alloc_bytes(additional_bytes_needed,
                           detail::nearest_alignment_exponent(m.blocksize), 0);
    // TODO: it is possible here to try allocating a set of smaller blocks from
    // the parent, absolutely necessary for when the total blocks of the
    // allocator grows and even 0.5 of it is a very large contiguous allocation
    if (!res.okay()) [[unlikely]] {
        m.blocks->pop();
        return res.err();
    }

    m.memory = res.release_ref();
    m.blocks->end_unchecked() = m.memory;
    // otherwise we would need to point the thing at the end of the free list to
    // us
    assert(m.blocks_free == 0);

    for (size_t i = 0; i < additional_blocks_needed; ++i) {
        uint8_t* head = m.memory.data() + (i * m.blocksize);
        *reinterpret_cast<void**>(head) = head + m.blocksize;
    }

    m.total_blocks += additional_blocks_needed;
    m.blocks_free += additional_blocks_needed;
    m.last_freed = m.memory.data();

    return AllocationStatusCode::Okay;
}

#ifndef NDEBUG
ALLO_FUNC bool block_allocator_t::contains(bytes_t bytes) const noexcept
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

ALLO_FUNC allocation_result_t block_allocator_t::alloc_bytes(
    size_t bytes, uint8_t alignment_exponent, size_t typehash) noexcept
{
    // make sure it can even fit in a block
    assert(bytes <= m.blocksize);
    if (bytes > m.blocksize) [[unlikely]]
        return AllocationStatusCode::OOM;

    // check if we can guarantee that a given block will be aligned to the
    // requested exponent
    const uint8_t our_alignment =
        detail::nearest_alignment_exponent(m.blocksize);
    assert(our_alignment != 64);
    if (alignment_exponent > our_alignment) {
        return AllocationStatusCode::AllocationTooAligned;
    }

    // allocate a new set of blocks if necessary
    if (m.blocks_free == 0) [[unlikely]] {
        auto res = grow();
        if (!res.okay()) [[unlikely]]
            return res.err();
    }
    assert(m.blocks_free >= 1);

    void* const next_to_last_freed = *static_cast<void**>(m.last_freed);
#ifndef NDEBUG
    if (m.blocks_free > 1) {
        assert(contains(
            zl::raw_slice(*(uint8_t*)next_to_last_freed, m.blocksize)));
    }
#endif

    bytes_t chosen_block =
        zl::raw_slice<uint8_t>(*(uint8_t*)m.last_freed, m.blocksize);

    m.last_freed = next_to_last_freed;
    --m.blocks_free;

    // try to insert the typehash after the allocation
    if (typehash != 0) {
        if (auto* const typehash_location =
                get_location_for_typehash(chosen_block.data(), bytes)) {
            *typehash_location = typehash;
        }
        // otherwise, there is no room for the typehash, and we just make the
        // allocation untyped.
    }

    assert(chosen_block.size() >= bytes);
    return allocation_result_t(std::in_place, chosen_block, 0, bytes);
}

#ifndef ALLO_DISABLE_TYPEINFO
ALLO_FUNC size_t*
block_allocator_t::get_location_for_typehash(uint8_t* blockhead,
                                             size_t allocsize) const noexcept
{
    assert(allocsize <= m.blocksize);
    uint8_t* head = blockhead;
    head += allocsize;
    void* headvoid = head;
    size_t space_remaining = m.blocksize - allocsize;
    if (std::align(alignof(size_t), sizeof(size_t), headvoid,
                   space_remaining)) {
        return static_cast<size_t*>(headvoid);
    }
    return nullptr;
}
#endif

ALLO_FUNC allocation_result_t
block_allocator_t::remap_bytes(bytes_t mem, size_t old_typehash,
                               size_t new_size, size_t new_typehash) noexcept
{
    if (new_size > m.blocksize) {
        return AllocationStatusCode::OOM;
    }
    if (old_typehash != 0) {
        if (auto* typehash_location =
                get_location_for_typehash(mem.data(), mem.size())) {
            if (*typehash_location != old_typehash) {
                return AllocationStatusCode::InvalidType;
            }
        }
    }
    if (new_typehash != 0) {
        if (auto* typehash_location =
                get_location_for_typehash(mem.data(), new_size)) {
            *typehash_location = new_typehash;
        }
    }
    return zl::raw_slice(*mem.data(), new_size);
}

// TODO: redo free bytes to work with blocks
ALLO_FUNC allocation_status_t
block_allocator_t::free_bytes(bytes_t mem, size_t typehash) noexcept
{
    auto checkerr = free_status(mem, typehash);
    if (!checkerr.okay())
        return checkerr;

    *reinterpret_cast<void**>(mem.data()) = m.last_freed;
    m.last_freed = mem.data();

    return AllocationStatusCode::Okay;
}

ALLO_FUNC allocation_status_t
block_allocator_t::free_status(bytes_t mem, size_t typehash) const noexcept
{
#ifndef ALLO_DISABLE_TYPEINFO
    if (typehash != 0) {
        if (size_t* original_typehash =
                get_location_for_typehash(mem.data(), mem.size())) {
            if (*original_typehash != typehash) {
                return AllocationStatusCode::InvalidType;
            }
        }
    }
#endif
    if (mem.size() > m.blocksize)
        return AllocationStatusCode::MemoryInvalid;

    assert(contains(mem));

    return AllocationStatusCode::Okay;
}

ALLO_FUNC allocation_status_t block_allocator_t::register_destruction_callback(
    destruction_callback_t callback, void* user_data) noexcept
{
    if (!callback) {
        assert(callback != nullptr);
        return AllocationStatusCode::InvalidArgument;
    }

    // allocate a new block if necessary
    if (m.last_callback_array == nullptr ||
        m.last_callback_array_size >= max_destruction_entries_per_block()) {
        // NOTE: using 0 alignment exponent. relies on the block allocator
        // having blocks which are aligned enough for a destruction callback
        // array
        auto res =
            alloc_bytes(detail::calculate_bytes_needed_for_destruction_callback(
                            max_destruction_entries_per_block()),
                        0, 0);
        if (!res.okay()) [[unlikely]]
            return res.err();

        auto& callback_array =
            *reinterpret_cast<detail::destruction_callback_entry_list_node_t*>(
                res.release_ref().data());
        callback_array.prev = m.last_callback_array;
        m.last_callback_array = &callback_array;
        m.last_callback_array_size = 0;
    }

    assert(m.last_callback_array &&
           m.last_callback_array_size < max_destruction_entries_per_block());

    // actually insert the callback into the array
    m.last_callback_array->entries[m.last_callback_array_size] =
        detail::destruction_callback_entry_t{
            .callback = callback,
            .user_data = user_data,
        };
    ++m.last_callback_array_size;
    return AllocationStatusCode::Okay;
}
} // namespace allo
