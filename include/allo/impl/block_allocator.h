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
    if (m.parent) {
        call_all_destruction_callbacks();
        m.parent.value().free_bytes(m.mem, 0);
        m.blocks_free = 0;
        m.parent.reset();
    }
}

ALLO_FUNC
block_allocator_t::block_allocator_t(block_allocator_t&& other) noexcept
    : m(std::move(other.m))
{
    m_type = other.m_type;
    other.m.parent.reset();
    other.m.blocks_free = 0;
}

ALLO_FUNC void
block_allocator_t::call_all_destruction_callbacks() const noexcept
{
    if (m.num_destruction_array_blocks == 0)
        return;

    size_t darray_index = m.current_destruction_array_index;
    auto* const darray = reinterpret_cast<destruction_callback_array_t*>(
        &m.mem.data()[darray_index * m.blocksize]);
    for (size_t i = 0; i < m.current_destruction_array_size; ++i) {
        darray->entries[i].callback(darray->entries[i].user_data);
    }
    darray_index = darray->previous_index;
    for (size_t i = 0; i < m.num_destruction_array_blocks - 1; ++i) {
        auto* const darray = reinterpret_cast<destruction_callback_array_t*>(
            &m.mem.data()[darray_index * m.blocksize]);
        // all of the remaining destruction arrays should be full
        for (size_t i = 0; i < m.max_destruction_entries_per_block; ++i) {
            darray->entries[i].callback(darray->entries[i].user_data);
        }
        darray_index = darray->previous_index;
    }
}

ALLO_FUNC zl::res<block_allocator_t, AllocationStatusCode>
block_allocator_t::make_inner(
    bytes_t memory, zl::opt<detail::abstract_heap_allocator_t&> parent,
    size_t blocksize) noexcept
{
    // blocksize must be at least 8 bytes
    size_t actual_blocksize =
        (blocksize < minimum_blocksize) ? minimum_blocksize : blocksize;

    // find the biggest alignment that we can guarantee all elements of the
    // array will have.
    const auto blocksize_alignment = static_cast<size_t>(
        std::pow(2, detail::nearest_alignment_exponent(actual_blocksize)));
    // NOTE: here we are getting the nearest alignment exponent of a pointer,
    // which is generally a bad idea but in this case we are accounting for the
    // fact that it could be far bigger than expected
    const auto parent_alignment = static_cast<size_t>(
        std::pow(2, detail::nearest_alignment_exponent((size_t)memory.data())));
    const size_t alignment = parent_alignment > blocksize_alignment
                                 ? blocksize_alignment
                                 : parent_alignment;

#ifndef NDEBUG
    if (parent) {
        assert(parent.value().free_status(memory, 0).okay());
    }
#endif

    auto num_blocks =
        static_cast<size_t>(std::floor(static_cast<double>(memory.size()) /
                                       static_cast<double>(actual_blocksize)));
    assert(num_blocks < memory.size() + 1);

    if (num_blocks == 0)
        return AllocationStatusCode::OOM;

    size_t max_destruction_entries_per_block =
        (actual_blocksize - sizeof(destruction_callback_array_t)) /
        sizeof(destruction_callback_entry_t);

    assert(max_destruction_entries_per_block >= 1);

    for (size_t i = 0; i < num_blocks; ++i) {
        uint8_t* head = memory.data() + (i * actual_blocksize);
        assert(detail::nearest_alignment_exponent((size_t)head) >= 3);
        *reinterpret_cast<size_t*>(head) = i + 1;
    }

    return zl::res<block_allocator_t, AllocationStatusCode>(
        std::in_place,
        M{
            // parent allocator. if null, we don't own the allocation
            .parent = parent,
            // block of memory
            .mem = memory,
            // last freed index (the first block is free at start)
            .last_freed_index = 0,
            // blocks free (all are free at start)
            .blocks_free = num_blocks,
            // blocksize
            .blocksize = actual_blocksize,
            // max destruction entries per block
            .max_destruction_entries_per_block =
                max_destruction_entries_per_block,
            // number of destruction blocks
            .num_destruction_array_blocks = 0,
            // current destruction array index,
            .current_destruction_array_index = 0,
            // size of current destruction array
            .current_destruction_array_size = 0,
        });
}

ALLO_FUNC allocation_result_t block_allocator_t::alloc_bytes(
    size_t bytes, uint8_t alignment_exponent, size_t typehash) noexcept
{
    if (m.blocks_free == 0) {
        this->remap();
        if (m.blocks_free == 0) {
            return AllocationStatusCode::OOM;
        }
    }

    if (bytes > m.blocksize) {
        return AllocationStatusCode::OOM;
    }

    const uint8_t our_alignment =
        detail::nearest_alignment_exponent(m.blocksize);
    assert(our_alignment != 64);
    if (alignment_exponent > our_alignment) {
        return AllocationStatusCode::AllocationTooAligned;
    }

    size_t next_to_last_freed = *reinterpret_cast<size_t*>(
        &m.mem.data()[m.last_freed_index * m.blocksize]);
    if (next_to_last_freed > (m.mem.size() / m.blocksize)) {
        return AllocationStatusCode::Corruption;
    }

    bytes_t chosen_block(m.mem, m.last_freed_index * m.blocksize,
                         (m.last_freed_index + 1) * m.blocksize);

    m.last_freed_index = next_to_last_freed;
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

ALLO_FUNC allocation_result_t
block_allocator_t::remap_bytes(bytes_t mem, size_t old_typehash,
                               size_t new_size, size_t new_typehash) noexcept
{
    if (new_size > m.blocksize) {
        return AllocationStatusCode::AllocationTooAligned;
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

ALLO_FUNC allocation_status_t
block_allocator_t::free_bytes(bytes_t mem, size_t typehash) noexcept
{
    auto manalysis = try_analyze_block(mem, typehash);
    if (!manalysis.okay()) {
        return manalysis.err();
    }

    const auto analysis = manalysis.release();

    *reinterpret_cast<size_t*>(analysis.first_byte) = m.last_freed_index;
    m.last_freed_index = analysis.block_index;

    return AllocationStatusCode::Okay;
}

ALLO_FUNC zl::res<block_allocator_t::block_analysis_t, AllocationStatusCode>
block_allocator_t::try_analyze_block(bytes_t mem,
                                     size_t typehash) const noexcept
{
    // memory outside of allocator, or impossible to have allocated with this
    // allocator
    if (mem.data() < m.mem.data() || mem.size() > m.blocksize)
        return AllocationStatusCode::MemoryInvalid;

    block_analysis_t result;
    result.byte_index = mem.data() - m.mem.data();

    // memory is not correctly aligned
    if (result.byte_index % m.blocksize != 0)
        return AllocationStatusCode::MemoryInvalid;

    result.block_index = result.byte_index / m.blocksize;
    result.first_byte = &m.mem.data()[result.block_index];

    if (typehash != 0) {
        if (size_t* original_typehash =
                get_location_for_typehash(result.first_byte, mem.size())) {
            if (*original_typehash != typehash) {
                return AllocationStatusCode::InvalidType;
            }
        }
        // otherwise, a typehash wouldnt have fit on this allocation and we just
        // dont typecheck it
    }

    return result;
}

ALLO_FUNC allocation_status_t
block_allocator_t::free_status(bytes_t mem, size_t typehash) const noexcept
{
    return try_analyze_block(mem, typehash).err();
}

ALLO_FUNC allocation_status_t block_allocator_t::register_destruction_callback(
    destruction_callback_t callback, void* user_data) noexcept
{
    if (!callback) {
        return AllocationStatusCode::InvalidArgument;
    }
    bool has_arrays = m.num_destruction_array_blocks > 0;
    if (has_arrays && m.current_destruction_array_size !=
                          m.max_destruction_entries_per_block) {
        assert(m.current_destruction_array_size <
               m.max_destruction_entries_per_block);
        // there is space available inside the current destruction array, add to
        // it
        auto* array = reinterpret_cast<destruction_callback_array_t*>(
            &m.mem.data()[m.current_destruction_array_index * m.blocksize]);
        array->entries[m.current_destruction_array_size] = {callback,
                                                            user_data};
        ++m.current_destruction_array_size;
    } else {
        if (m.blocks_free == 0) {
            this->remap();
            if (m.blocks_free == 0) {
                return AllocationStatusCode::OOM;
            }
        }
        const size_t free_index = m.last_freed_index;
        void* const freeblock = &m.mem.data()[free_index * m.blocksize];
        const size_t next_to_last_freed = *reinterpret_cast<size_t*>(freeblock);
        auto* const new_array =
            reinterpret_cast<destruction_callback_array_t*>(freeblock);
        --m.blocks_free;
        ++m.num_destruction_array_blocks;
        m.last_freed_index = next_to_last_freed;
        new_array->previous_index = m.current_destruction_array_index;
        m.current_destruction_array_index = free_index;
        m.current_destruction_array_size = 1;
        new_array->entries[0] = {callback, user_data};
    }
    return AllocationStatusCode::Okay;
}

ALLO_FUNC allocation_status_t block_allocator_t::remap() noexcept
{
    auto res = m.parent.value().remap_bytes(
        m.mem, 0,
        std::ceil(reallocation_ratio * static_cast<double>(m.mem.size())), 0);
    if (!res.okay())
        return res.err();
    const auto original_num_blocks = static_cast<size_t>(std::floor(
        static_cast<double>(m.mem.size()) / static_cast<double>(m.blocksize)));
    m.mem = res.release();
    const auto num_blocks = static_cast<size_t>(std::floor(
        static_cast<double>(m.mem.size()) / static_cast<double>(m.blocksize)));
    assert(original_num_blocks < num_blocks);
    size_t difference = num_blocks - original_num_blocks;
    size_t next_available = m.last_freed_index;
    // NOTE: if we assert that there is always at least one block before
    // realloc, we can use size_t for iterator instead of int64_t
    for (int64_t i = static_cast<int64_t>(num_blocks) - 1;
         i >= original_num_blocks; --i) {
        uint8_t* first_byte_of_new_block = m.mem.data() + (m.blocksize * i);
        assert(detail::nearest_alignment_exponent(
                   (size_t)first_byte_of_new_block) >= 3);
        *reinterpret_cast<size_t*>(first_byte_of_new_block) = next_available;
        // the next block we iterate over will be pointing to this one
        next_available = i;
    }
    m.last_freed_index = original_num_blocks;
    m.blocks_free += difference;
    return AllocationStatusCode::Okay;
}
} // namespace allo
