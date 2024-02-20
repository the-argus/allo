#pragma once
#include "allo/allocator_interfaces.h"
#include "allo/impl/block_allocator.h"
#include "ziglike/slice.h"
#ifndef ALLO_HEADER_ONLY
#ifndef ALLO_OVERRIDE_IMPL_INCLUSION_GUARD
#error \
    "Attempt to include allo/impl header file but header-only mode is not enabled."
#endif
#endif

#include "allo/stack_allocator.h"
#include "ziglike/defer.h"
#include <cstring>
#include <memory>

#ifdef ALLO_HEADER_ONLY
#ifndef ALLO_FUNC
#define ALLO_FUNC inline
#endif
#else
#define ALLO_FUNC
#endif

namespace allo {

ALLO_FUNC stack_allocator_t::~stack_allocator_t() noexcept
{
    // call all destruction callbacks
    // now that callbacks are called, free memory
    m.parent.free_bytes(m.memory, 0);
}

ALLO_FUNC
stack_allocator_t::stack_allocator_t(stack_allocator_t &&other) noexcept
    : m(other.m)
{
    type = enum_value;
}

[[nodiscard]] ALLO_FUNC allocation_result_t stack_allocator_t::alloc_bytes(
    size_t bytes, uint8_t alignment_exponent, size_t typehash)
{
    if (bytes == 0) {
        return AllocationStatusCode::InvalidArgument;
    }

    auto alignment = static_cast<size_t>(std::pow(2, alignment_exponent));
    if (alignment > get_max_alignment(properties())) {
        return AllocationStatusCode::AllocationTooAligned;
    }
    zl::slice<uint8_t> original_available = m.available_memory;

    auto *bookkeeping = static_cast<previous_state_t *>(
        raw_alloc(alignof(previous_state_t), sizeof(previous_state_t)));

    if (!bookkeeping) [[unlikely]] {
        return AllocationStatusCode::OOM;
    }

    zl::defer free_bookkeeping(
        [this, original_available]() { m.memory = original_available; });

    *bookkeeping = {
        .stack_top =
            static_cast<size_t>(original_available.data() - m.memory.data()),
        .type_hashcode = m.last_type_hashcode,
    };

    void *actual = raw_alloc(alignment, bytes);
    // if second alloc fails, undo the first one
    if (!actual) [[unlikely]] {
        return AllocationStatusCode::OOM;
    }
    free_bookkeeping.cancel();

    m.last_type_hashcode = typehash;

    return zl::raw_slice(*static_cast<uint8_t *>(actual), bytes);
}

ALLO_FUNC void *stack_allocator_t::raw_alloc(size_t align,
                                             size_t typesize) ALLO_NOEXCEPT
{
    // these will get modified in place by std::align
    void *new_available_start = m.available_memory.data();
    size_t new_size = m.available_memory.size();
    if (std::align(align, typesize, new_available_start, new_size)) {
        auto *available_after_alloc =
            static_cast<uint8_t *>(new_available_start);

        available_after_alloc += typesize;
        m.available_memory = zl::slice<uint8_t>(
            m.available_memory, m.available_memory.size() - new_size,
            m.available_memory.size());

        return new_available_start;
    }
    return nullptr;
}

ALLO_FUNC allocation_status_t
stack_allocator_t::free_status(zl::slice<uint8_t> mem, size_t typehash) const
{
    return free_common(mem, typehash).err();
}

ALLO_FUNC zl::res<stack_allocator_t::previous_state_t &, AllocationStatusCode>
stack_allocator_t::free_common(zl::slice<uint8_t> mem,
                               size_t typehash) const noexcept
{
    if (m.last_type_hashcode != typehash)
        return AllocationStatusCode::InvalidType;

    if (mem.data() < m.memory.data() ||
        mem.data() + mem.size() > m.memory.data() + m.memory.size()) {
        return AllocationStatusCode::MemoryInvalid;
    }

    void *item = mem.data();
    // retrieve the bookeeping data from behind the given allocation
    void *bookkeeping_aligned = item;
    size_t size =
        (m.memory.data() + m.memory.size()) - static_cast<uint8_t *>(item);

    if (!std::align(alignof(previous_state_t), sizeof(previous_state_t),
                    bookkeeping_aligned, size)) [[unlikely]] {
        return AllocationStatusCode::OOM;
    }

    // this should be the location of where you could next put a bookkeeping
    // object
    assert(bookkeeping_aligned >= item);
    static_assert(alignof(size_t) == alignof(previous_state_t));
    auto *maybe_bookkeeping = static_cast<size_t *>(bookkeeping_aligned);
    // now move backwards until we get to a point where we're in a valid
    // previous_state_t-sized space
    while ((uint8_t *)maybe_bookkeeping + sizeof(previous_state_t) > item) {
        --maybe_bookkeeping;
    }

    auto *bookkeeping = reinterpret_cast<previous_state_t *>(maybe_bookkeeping);

    // try to detect invalid or corrupted memory. happens when you free a type
    // other than the last one to be allocated
    {
        const bool too_big = bookkeeping->stack_top >=
                             m.available_memory.data() - m.memory.data();
        const bool before_begin =
            reinterpret_cast<uint8_t *>(maybe_bookkeeping) < m.memory.data();
        const bool after_end = reinterpret_cast<uint8_t *>(maybe_bookkeeping) >=
                               m.memory.data() + m.memory.size();
        if (too_big || before_begin || after_end) {
            return AllocationStatusCode::Corruption;
        }
    }

    return *bookkeeping;
}

ALLO_FUNC allocation_status_t
stack_allocator_t::free_bytes(zl::slice<uint8_t> mem, size_t typehash)
{
    auto mbookkeeping = free_common(mem, typehash);
    if (!mbookkeeping.okay())
        return mbookkeeping.err();
    // actually modify the container
    auto &bookkeeping = mbookkeeping.release();
    m.available_memory =
        zl::slice<uint8_t>(m.memory, bookkeeping.stack_top,
                           m.available_memory.end().ptr() - m.memory.data());
    m.last_type_hashcode = bookkeeping.type_hashcode;
    return AllocationStatusCode::Okay;
}

ALLO_FUNC allocation_result_t
stack_allocator_t::realloc_bytes(zl::slice<uint8_t> mem, size_t old_typehash,
                                 size_t new_size, size_t new_typehash)
{
    if (old_typehash != m.last_type_hashcode)
        return AllocationStatusCode::InvalidArgument;

    // always err
    return AllocationStatusCode::OOM;
}

ALLO_FUNC zl::res<stack_allocator_t, AllocationStatusCode>
stack_allocator_t::make(zl::slice<uint8_t> memory,
                        allocator_with<IRealloc, IFree> &parent) ALLO_NOEXCEPT
{
    // make sure there is at least one byte of space to be allocated in memory
    if (memory.size() <= sizeof(previous_state_t)) {
        return AllocationStatusCode::InvalidArgument;
    }

    assert(parent.free_status(memory, 0).okay());

    return stack_allocator_t(M{
        parent,
        memory,
        memory,
        0,
        make_properties(memory.size(), alignof(previous_state_t)),
    });
}

ALLO_FUNC const allocator_properties_t &stack_allocator_t::properties() const
{
    return m.properties;
}

ALLO_FUNC allocation_status_t stack_allocator_t::register_destruction_callback(
    destruction_callback_t callback, void *user_data) noexcept
{
    if (!callback) {
        return AllocationStatusCode::InvalidArgument;
    }
    // not valid/used in this case, except that if a
    // destruction_callback_entry_t doesn't fit in the available space,
    // std::align will return nullptr
    size_t dummy_size = m.available_memory.size();
    void *head = m.available_memory.end().ptr();
    if (auto *aligned = (destruction_callback_entry_t *)std::align(
            alignof(destruction_callback_entry_t),
            sizeof(destruction_callback_entry_t), head, dummy_size)) {
        while ((void *)(aligned + 1) > m.available_memory.end().ptr()) {
            --aligned;
        }

        if ((void *)aligned < (void *)m.available_memory.data()) {
            return AllocationStatusCode::OOM;
        }

        m.available_memory = zl::slice<uint8_t>(
            m.available_memory, 0,
            (uint8_t *)aligned - (uint8_t *)m.available_memory.data());

        *aligned = {callback, user_data};
        return AllocationStatusCode::Okay;
    }
    return AllocationStatusCode::OOM;
}
} // namespace allo
