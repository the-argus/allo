#pragma once
#ifndef ALLO_HEADER_ONLY
#ifndef ALLO_OVERRIDE_IMPL_INCLUSION_GUARD
#error \
    "Attempt to include allo/impl header file but header-only mode is not enabled."
#endif
#endif

#include "allo/stack_allocator.h"
#include "ziglike/defer.h"
#include "ziglike/slice.h"
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
    auto *entry = reinterpret_cast<destruction_callback_entry_t *>(
        m.available_memory.end().ptr());
    while ((uint8_t *)(entry + 1) <= m.memory.end().ptr()) {
        entry->callback(entry->user_data);
        ++entry;
    }
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
    size_t bytes, uint8_t alignment_exponent, size_t typehash) noexcept
{
    if (bytes == 0) {
        return AllocationStatusCode::InvalidArgument;
    }

    auto alignment = static_cast<size_t>(std::pow(2, alignment_exponent));
    if (alignment > properties().m_maximum_alignment) {
        return AllocationStatusCode::AllocationTooAligned;
    }
    zl::slice<uint8_t> original_available = m.available_memory;

    previous_state_t *bookkeeping = nullptr;

    auto allocate_bookkeeping = [this, &bookkeeping]() {
        bookkeeping = static_cast<previous_state_t *>(
            raw_alloc(alignof(previous_state_t), sizeof(previous_state_t)));
    };
    allocate_bookkeeping();

    if (!bookkeeping) [[unlikely]] {
        const allocation_status_t status = this->realloc();
        if (!status.okay())
            return status.err();
        allocate_bookkeeping();
        if (!bookkeeping) [[unlikely]]
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
    if (!actual) [[unlikely]] {
        const allocation_status_t status = this->realloc();
        if (!status.okay()) [[unlikely]]
            return status.err();

        actual = raw_alloc(alignment, bytes);

        assert(actual);
        if (!actual) [[unlikely]]
            return AllocationStatusCode::OOM;
    }

    assert(actual != bookkeeping);
    // success, no way we can err now, so just
    free_bookkeeping.cancel();

    m.last_type_hashcode = typehash;

    return zl::raw_slice(*static_cast<uint8_t *>(actual), bytes);
}

ALLO_FUNC void *stack_allocator_t::raw_alloc(size_t align,
                                             size_t typesize) noexcept
{
    // these will get modified in place by std::align
    void *new_available_start = m.available_memory.data();
    size_t new_size = m.available_memory.size();
    if (std::align(align, typesize, new_available_start, new_size)) {
        if (new_available_start >= (m.memory.end().ptr() - typesize)) {
            return nullptr;
        }
        m.available_memory = zl::slice<uint8_t>(m.available_memory,
                                                m.available_memory.size() -
                                                    (new_size - typesize),
                                                m.available_memory.size());

        return new_available_start;
    }
    return nullptr;
}

ALLO_FUNC allocation_status_t stack_allocator_t::realloc() noexcept
{
    auto result = m.parent.realloc_bytes(
        m.memory, 0,
        size_t(std::ceil(static_cast<double>(m.memory.size()) *
                         reallocation_ratio)),
        0);

    if (!result.okay())
        return result.err();

    const zl::slice<uint8_t> &newmem = result.release_ref();
#ifndef NDEBUG
    if (newmem.data() != m.memory.data()) {
        // this shouldnt happen, reallocation is supposed to be stable
        std::abort();
    }
#endif

    m.memory = newmem;
    return AllocationStatusCode::Okay;
}

ALLO_FUNC allocation_status_t stack_allocator_t::free_status(
    zl::slice<uint8_t> mem, size_t typehash) const noexcept
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
        sizeof(previous_state_t) * 2; // always pretend its large enough

    std::align(alignof(previous_state_t), sizeof(previous_state_t),
               bookkeeping_aligned, size);
    assert(bookkeeping_aligned);

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
    assert((void *)bookkeeping >= m.memory.data() &&
           (void *)bookkeeping < m.memory.end().ptr());

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
stack_allocator_t::free_bytes(zl::slice<uint8_t> mem, size_t typehash) noexcept
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
                                 size_t new_size, size_t new_typehash) noexcept
{
    if (old_typehash != m.last_type_hashcode)
        return AllocationStatusCode::InvalidArgument;

    // always err
    return AllocationStatusCode::OOM;
}

ALLO_FUNC zl::res<stack_allocator_t, AllocationStatusCode>
stack_allocator_t::make_inner(zl::slice<uint8_t> memory,
                              DynamicHeapAllocatorRef parent) ALLO_NOEXCEPT
{
    // make sure there is at least one byte of space to be allocated in memory
    if (memory.size() <= sizeof(previous_state_t)) {
        return AllocationStatusCode::InvalidArgument;
    }

    assert(parent.free_status(memory, 0).okay());

    return zl::res<stack_allocator_t, AllocationStatusCode>{
        std::in_place,
        M{
            parent,
            memory,
            memory,
            0,
            allocator_properties_t(memory.size(), alignof(previous_state_t)),
        }};
}

ALLO_FUNC const allocator_properties_t &
stack_allocator_t::properties() const noexcept
{
    return m.properties;
}

ALLO_FUNC allocation_status_t stack_allocator_t::register_destruction_callback(
    destruction_callback_t callback, void *user_data) noexcept
{
    if (!callback) {
        return AllocationStatusCode::InvalidArgument;
    }
    auto try_alloc = [this, callback, user_data]() -> allocation_status_t {
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
    };

    if (!try_alloc().okay()) [[unlikely]] {
        const allocation_status_t realloc_status = this->realloc();
        if (!realloc_status.okay()) [[unlikely]]
            return realloc_status.err();
        const allocation_status_t status = try_alloc();
        if (!status.okay()) [[unlikely]]
            return status.err();
    }
    return AllocationStatusCode::Okay;
}
} // namespace allo
