#include "allo/stack_allocator.hpp"
#include <cstring>
#include <memory>

namespace allo {

void *stack_allocator_t::inner_alloc(size_t align,
                                     size_t typesize) TESTING_NOEXCEPT
{
    assert(owning_thread == std::this_thread::get_id());
    size_t original_available = m_first_available;
    auto *bookkeeping = static_cast<previous_state_t *>(
        raw_alloc(alignof(previous_state_t), sizeof(previous_state_t)));
    if (!bookkeeping) [[unlikely]] {
#ifdef ALLO_LOGGING
        LN_WARN("unable to allocate bookkeeping data");
#endif
        return nullptr;
    }
    *bookkeeping = {
        .memory_available = original_available,
        .type_hashcode = m_last_type,
    };

    void *actual = raw_alloc(align, typesize);
    // if second alloc fails, undo the first one
    if (!actual) [[unlikely]] {
#ifdef ALLO_LOGGING
        LN_WARN("Successfully allocated bookkeeping data but unable to "
                "allocate actual payload data");
#endif
        m_first_available = original_available;
        return nullptr;
    }
    return actual;
}

void *stack_allocator_t::raw_alloc(size_t align,
                                   size_t typesize) TESTING_NOEXCEPT
{
    assert(owning_thread == std::this_thread::get_id());
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

void *stack_allocator_t::inner_free(size_t align, size_t typesize,
                                    void *item) TESTING_NOEXCEPT
{
    assert(owning_thread == std::this_thread::get_id());

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

void stack_allocator_t::zero() TESTING_NOEXCEPT
{
    assert(owning_thread == std::this_thread::get_id());
    std::memset(m_memory.data(), 0, m_memory.size());
}

stack_allocator_t::stack_allocator_t(stack_allocator_t &&other) TESTING_NOEXCEPT
    : m_memory(other.m_memory)
{
    assert(other.owning_thread == std::this_thread::get_id());
#ifndef NDEBUG
    owning_thread = other.owning_thread;
#endif
}

stack_allocator_t &
stack_allocator_t::operator=(stack_allocator_t &&other) TESTING_NOEXCEPT
{
    assert(owning_thread == std::this_thread::get_id());
    if (&other == this) [[unlikely]]
        return *this;
    m_memory = other.m_memory;
    m_first_available = other.m_first_available;
    return *this;
}

// mark all memory as available
stack_allocator_t::stack_allocator_t(lib::slice_t<uint8_t> memory)
    TESTING_NOEXCEPT : m_memory(memory)
{
#ifndef NDEBUG
    owning_thread = std::this_thread::get_id();
#endif
}
} // namespace allo
