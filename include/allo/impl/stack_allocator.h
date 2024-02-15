#pragma once
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

[[nodiscard]] ALLO_FUNC allocation_result_t
stack_allocator_t::alloc_bytes(size_t bytes, size_t alignment, size_t typehash)
{
    size_t original_available = m_first_available;

    auto *bookkeeping = static_cast<previous_state_t *>(
        raw_alloc(alignof(previous_state_t), sizeof(previous_state_t)));

    if (!bookkeeping) [[unlikely]] {
        return AllocationStatusCode::OOM;
    }

    zl::defer free_bookkeeping([this, original_available]() {
        m_first_available = original_available;
    });

    *bookkeeping = {
        .memory_available = original_available,
        .type_hashcode = m_last_type,
    };

    void *actual = raw_alloc(alignment, bytes);
    // if second alloc fails, undo the first one
    if (!actual) [[unlikely]] {
        return AllocationStatusCode::OOM;
    }
    free_bookkeeping.cancel();

    m_last_type = typehash;

    return zl::raw_slice(*static_cast<uint8_t *>(actual), bytes);
}

ALLO_FUNC void *stack_allocator_t::raw_alloc(size_t align,
                                             size_t typesize) ALLO_NOEXCEPT
{
    // these will get modified in place by std::align
    void *new_available_start = m_memory.data() + m_first_available;
    size_t new_size = m_memory.size() - m_first_available;
    if (std::align(align, typesize, new_available_start, new_size)) {
        auto *available_after_alloc =
            static_cast<uint8_t *>(new_available_start);

        available_after_alloc += typesize;
        m_first_available = available_after_alloc - m_memory.data();

        return new_available_start;
    }
    return nullptr;
}

[[nodiscard]] allocation_status_t
stack_allocator_t::free_status(zl::slice<uint8_t> mem, size_t typehash) const
{
    return free_common(mem, typehash).err();
}

zl::res<stack_allocator_t::previous_state_t &, AllocationStatusCode>
stack_allocator_t::free_common(zl::slice<uint8_t> mem,
                               size_t typehash) const noexcept
{
    if (m_last_type != typehash)
        return AllocationStatusCode::InvalidArgument;

    if (mem.data() < m_memory.data() ||
        mem.data() + mem.size() > m_memory.data() + m_memory.size()) {
        return AllocationStatusCode::InvalidArgument;
    }

    void *item = mem.data();
    // retrieve the bookeeping data from behind the given allocation
    void *bookkeeping_aligned = item;
    size_t size =
        (m_memory.data() + m_memory.size()) - static_cast<uint8_t *>(item);

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
        const bool too_big = bookkeeping->memory_available >= m_memory.size();
        const bool before_begin =
            reinterpret_cast<uint8_t *>(maybe_bookkeeping) < m_memory.data();
        const bool after_end = reinterpret_cast<uint8_t *>(maybe_bookkeeping) >=
                               m_memory.data() + m_memory.size();
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
    m_first_available = bookkeeping.memory_available;
    m_last_type = bookkeeping.type_hashcode;
    return AllocationStatusCode::Okay;
}

allocation_result_t stack_allocator_t::realloc_bytes(zl::slice<uint8_t> mem,
                                                     size_t new_size,
                                                     size_t typehash)
{
    if (typehash != m_last_type)
        return AllocationStatusCode::InvalidArgument;

    // always err
    return AllocationStatusCode::OOM;
}

ALLO_FUNC void stack_allocator_t::zero() ALLO_NOEXCEPT
{
    std::memset(m_memory.data(), 0, m_memory.size());
}

stack_allocator_t::stack_allocator_t(stack_allocator_t &&other) noexcept
    : m_memory(other.m_memory),
      m_properties(make_properties(other.m_memory.size(), 8))
{
    type = detail::AllocatorType::StackAllocator;
}

stack_allocator_t &
stack_allocator_t::operator=(stack_allocator_t &&other) noexcept
{
    if (&other == this) [[unlikely]]
        return *this;
    m_memory = other.m_memory;
    m_first_available = other.m_first_available;
    return *this;
}

// mark all memory as available
stack_allocator_t::stack_allocator_t(zl::slice<uint8_t> memory) ALLO_NOEXCEPT
    : m_memory(memory),
      m_properties(make_properties(memory.size(), 8))
{
    type = detail::AllocatorType::StackAllocator;
}

ALLO_FUNC allocation_status_t stack_allocator_t::register_destruction_callback(
    destruction_callback_t callback, void *user_data) noexcept
{
    return AllocationStatusCode::InvalidArgument;
}
} // namespace allo
