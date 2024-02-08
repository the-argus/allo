#ifndef ALLO_HEADER_ONLY
#ifndef ALLO_OVERRIDE_IMPL_INCLUSION_GUARD
#error \
    "Attempt to include allo/impl header file but header-only mode is not enabled."
#endif
#endif

#include "allo/stack_allocator.h"
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

ALLO_FUNC void *stack_allocator_t::inner_alloc(size_t align,
                                               size_t typesize) ALLO_NOEXCEPT
{
    size_t original_available = m_first_available;
    auto *bookkeeping = static_cast<previous_state_t *>(
        raw_alloc(alignof(previous_state_t), sizeof(previous_state_t)));
    if (!bookkeeping) [[unlikely]] {
        return nullptr;
    }
    *bookkeeping = {
        .memory_available = original_available,
        .type_hashcode = m_last_type,
    };

    void *actual = raw_alloc(align, typesize);
    // if second alloc fails, undo the first one
    if (!actual) [[unlikely]] {
        m_first_available = original_available;
        return nullptr;
    }
    return actual;
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

ALLO_FUNC void *stack_allocator_t::inner_free(size_t align, size_t typesize,
                                              void *item) ALLO_NOEXCEPT
{
    // retrieve the bookeeping data from behind the given allocation
    void *bookkeeping_aligned = item;
    size_t size =
        (m_memory.data() + m_memory.size()) - static_cast<uint8_t *>(item);

    if (!std::align(alignof(previous_state_t), sizeof(previous_state_t),
                    bookkeeping_aligned, size)) [[unlikely]] {
        return nullptr;
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
    if (bookkeeping->memory_available >= m_memory.size() ||
        !(reinterpret_cast<uint8_t *>(maybe_bookkeeping) >= m_memory.data() &&
          reinterpret_cast<uint8_t *>(maybe_bookkeeping) <
              m_memory.data() + m_memory.size())) {
        return nullptr;
    }

    // found bookkeeping item! now we can read the memory amount
    m_first_available = bookkeeping->memory_available;
    m_last_type = bookkeeping->type_hashcode;
    return item;
}

ALLO_FUNC void stack_allocator_t::zero() ALLO_NOEXCEPT
{
    std::memset(m_memory.data(), 0, m_memory.size());
}

stack_allocator_t::stack_allocator_t(stack_allocator_t &&other) noexcept
    : m_memory(other.m_memory)
{
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
    : m_memory(memory)
{
}
} // namespace allo
