#pragma once

#include "allo/allocator_interfaces.h"
#include <cmath>
#ifndef ALLO_HEADER_ONLY
#ifndef ALLO_OVERRIDE_IMPL_INCLUSION_GUARD
#error \
    "Attempt to include allo/impl header file but header-only mode is not enabled."
#endif
#endif

#include "allo/block_allocator.h"

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
    if (m.owning) {
        call_all_destruction_callbacks();
        m.parent.free_bytes(m.mem, 0);
        m.blocks_free = 0;
        m.owning = false;
    }
}

ALLO_FUNC
block_allocator_t::block_allocator_t(block_allocator_t &&other) noexcept
    : m(other.m)
{
    other.m.owning = false;
    other.m.blocks_free = 0;
}

ALLO_FUNC void
block_allocator_t::call_all_destruction_callbacks() const noexcept
{
    if (m.num_destruction_array_blocks == 0)
        return;

    size_t darray_index = m.current_destruction_array_index;
    auto *const darray = reinterpret_cast<destruction_callback_array_t *>(
        &m.mem.data()[darray_index * m.blocksize]);
    for (size_t i = 0; i < m.current_destruction_array_size; ++i) {
        assert(darray->entries[i].user_data > (void *)m.mem.begin().ptr() &&
               darray->entries[i].user_data < m.mem.end().ptr());
        darray->entries[i].callback(darray->entries[i].user_data);
    }
    darray_index = darray->previous_index;
    for (size_t i = 0; i < m.num_destruction_array_blocks - 1; ++i) {
        auto *const darray = reinterpret_cast<destruction_callback_array_t *>(
            &m.mem.data()[darray_index * m.blocksize]);
        // all of the remaining destruction arrays should be full
        for (size_t i = 0; i < m.max_destruction_entries_per_block; ++i) {
            assert(darray->entries[i].user_data > (void *)m.mem.begin().ptr() &&
                   darray->entries[i].user_data < m.mem.end().ptr());
            darray->entries[i].callback(darray->entries[i].user_data);
        }
        darray_index = darray->previous_index;
    }
}

ALLO_FUNC zl::res<block_allocator_t, AllocationStatusCode>
block_allocator_t::make(const zl::slice<uint8_t> &memory,
                        allocator_with<IRealloc, IFree> &parent,
                        size_t blocksize, uint8_t alignment_exponent) noexcept
{
    // NOTE: this could be supported by adding padding, but it would make things
    // more complicated.
    auto alignment = static_cast<size_t>(std::pow(2, alignment_exponent));
    if (alignment > blocksize)
        return AllocationStatusCode::InvalidArgument;

    if (((uintptr_t)memory.data() & alignment - 1) != 0) {
        // memory given is not correctly aligned.
        return AllocationStatusCode::InvalidArgument;
    }

    // blocksize must be at least 8 bytes
    size_t actual_blocksize =
        (blocksize < minimum_blocksize) ? minimum_blocksize : blocksize;

    assert(parent.free_status(memory, 0).okay());

    auto num_blocks =
        static_cast<size_t>(std::floor(static_cast<double>(memory.size()) /
                                       static_cast<double>(actual_blocksize)));
    assert(num_blocks < memory.size() + 1);

    if (num_blocks == 0)
        return AllocationStatusCode::AllocationTooBig;

    size_t max_destruction_entries_per_block =
        (actual_blocksize - sizeof(destruction_callback_array_t)) /
        sizeof(destruction_callback_entry_t);

    assert(max_destruction_entries_per_block >= 1);

    for (size_t i = 0; i < num_blocks; ++i) {
        *reinterpret_cast<size_t *>(&memory.data()[i * actual_blocksize]) =
            i + 1;
    }

    return block_allocator_t(M{
        // parent allocator
        parent,
        // block of memory
        memory,
        // allocator properties
        make_properties(actual_blocksize, alignment),
        // last freed index (the first block is free at start)
        0,
        // blocks free (all are free at start)
        num_blocks,
        // blocksize
        actual_blocksize,
        // max destruction entries per block
        max_destruction_entries_per_block,
        // number of destruction blocks
        0,
        // current destruction array index,
        0,
        // size of current destruction array
        0,
        // whether we own our allocation
        true,
    });
}

ALLO_FUNC allocation_result_t block_allocator_t::alloc_bytes(
    size_t bytes, uint8_t alignment_exponent, size_t typehash)
{
    if (m.blocks_free == 0) {
        return AllocationStatusCode::OOM;
    }

    if (bytes > get_max_contiguous_bytes(m.properties)) {
        return AllocationStatusCode::AllocationTooBig;
    }

    auto alignment = static_cast<size_t>(std::pow(2, alignment_exponent));
    if (alignment > get_max_alignment(m.properties)) {
        return AllocationStatusCode::AllocationTooAligned;
    }

    size_t next_to_last_freed = *reinterpret_cast<size_t *>(
        &m.mem.data()[m.last_freed_index * m.blocksize]);
    assert(m.mem.size() % m.blocksize == 0);
    if (next_to_last_freed > (m.mem.size() / m.blocksize)) {
        return AllocationStatusCode::Corruption;
    }

    zl::slice<uint8_t> chosen_block(m.mem, m.last_freed_index * m.blocksize,
                                    (m.last_freed_index + 1) * m.blocksize);

    m.last_freed_index = next_to_last_freed;
    --m.blocks_free;

    // try to insert the typehash after the allocation
    if (typehash != 0) {
        if (auto *const typehash_location =
                get_location_for_typehash(chosen_block.data(), bytes)) {
            *typehash_location = typehash;
        }
        // otherwise, there is no room for the typehash, and we just make the
        // allocation untyped.
    }

    return chosen_block;
}

ALLO_FUNC size_t *
block_allocator_t::get_location_for_typehash(uint8_t *blockhead,
                                             size_t allocsize) const noexcept
{
    assert(allocsize <= m.blocksize);
    uint8_t *head = blockhead;
    head += allocsize;
    void *headvoid = head;
    size_t space_remaining = m.blocksize - allocsize;
    if (std::align(alignof(size_t), sizeof(size_t), headvoid,
                   space_remaining)) {
        return static_cast<size_t *>(headvoid);
    }
    return nullptr;
}

ALLO_FUNC allocation_result_t
block_allocator_t::realloc_bytes(zl::slice<uint8_t> mem, size_t old_typehash,
                                 size_t new_size, size_t new_typehash)
{
    if (new_size > m.blocksize) {
        return AllocationStatusCode::AllocationTooAligned;
    }
    if (old_typehash != 0) {
        if (auto *typehash_location =
                get_location_for_typehash(mem.data(), mem.size())) {
            if (*typehash_location != old_typehash) {
                return AllocationStatusCode::InvalidType;
            }
        }
    }
    if (new_typehash != 0) {
        if (auto *typehash_location =
                get_location_for_typehash(mem.data(), new_size)) {
            *typehash_location = new_typehash;
        }
    }
    return zl::raw_slice(*mem.data(), new_size);
}

ALLO_FUNC allocation_status_t
block_allocator_t::free_bytes(zl::slice<uint8_t> mem, size_t typehash)
{
    auto manalysis = try_analyze_block(mem, typehash);
    if (!manalysis.okay()) {
        return manalysis.err();
    }

    const auto analysis = manalysis.release();

    *reinterpret_cast<size_t *>(analysis.first_byte) = m.last_freed_index;
    m.last_freed_index = analysis.block_index;

    return AllocationStatusCode::Okay;
}

ALLO_FUNC zl::res<block_allocator_t::block_analysis_t, AllocationStatusCode>
block_allocator_t::try_analyze_block(zl::slice<uint8_t> mem,
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
        if (size_t *original_typehash =
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

ALLO_FUNC allocation_status_t block_allocator_t::free_status(
    zl::slice<uint8_t> mem, size_t typehash) const noexcept
{
    return try_analyze_block(mem, typehash).err();
}

ALLO_FUNC allocation_status_t block_allocator_t::register_destruction_callback(
    destruction_callback_t callback, void *user_data) noexcept
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
        auto *array = reinterpret_cast<destruction_callback_array_t *>(
            &m.mem.data()[m.current_destruction_array_index * m.blocksize]);
        array->entries[m.current_destruction_array_size] = {callback,
                                                            user_data};
        ++m.current_destruction_array_size;
    } else {
        if (m.blocks_free == 0) {
            return AllocationStatusCode::OOM;
        }
        const size_t free_index = m.last_freed_index;
        void *const freeblock = &m.mem.data()[free_index * m.blocksize];
        const size_t next_to_last_freed =
            *reinterpret_cast<size_t *>(freeblock);
        auto *const new_array =
            reinterpret_cast<destruction_callback_array_t *>(freeblock);
        --m.blocks_free;
        m.last_freed_index = next_to_last_freed;
        new_array->previous_index = m.current_destruction_array_index;
        m.current_destruction_array_index = free_index;
        m.current_destruction_array_size = 1;
        new_array->entries[0] = {callback, user_data};
    }
    return AllocationStatusCode::Okay;
}
} // namespace allo
