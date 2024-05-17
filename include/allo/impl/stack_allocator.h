#pragma once
#ifndef ALLO_HEADER_ONLY
#ifndef ALLO_OVERRIDE_IMPL_INCLUSION_GUARD
#error \
    "Attempt to include allo/impl header file but header-only mode is not enabled."
#endif
#endif

#include "allo/stack_allocator.h"
#include <cmath>
#include <cstring>
#include <memory>
#include <ziglike/defer.h>
#include <ziglike/slice.h>

#ifdef ALLO_HEADER_ONLY
#ifndef ALLO_FUNC
#define ALLO_FUNC inline
#endif
#else
#define ALLO_FUNC
#endif

namespace allo {

// NOTE: this function is identical to scratch allocator
ALLO_FUNC stack_allocator_t::~stack_allocator_t() noexcept
{
    // call all destruction callbacks
    {
        destruction_callback_entry_t* iter = m.last_callback;
        while (iter) {
            iter->callback(iter->user_data);
            iter = iter->prev;
        }
    }

    // free if we are owning
    if (!m.parent.is_heap())
        return;

    if (m.blocks) {
        while (auto iter = m.blocks->end()) {
            m.parent.get_heap_unchecked().free_bytes(iter.value(), 0);
            m.blocks->pop();
        }
    } else {
        m.parent.get_heap_unchecked().free_bytes(m.memory, 0);
    }
}

// NOTE: this function is identical to scratch allocator
ALLO_FUNC
stack_allocator_t::stack_allocator_t(stack_allocator_t&& other) noexcept
    : m(other.m)
{
    m_type = other.m_type;
    other.m.parent = any_allocator_t();
    other.m.last_callback = nullptr;
}

// round up to be an even integer exponential of 2, times the original
// size.
// NOTE: this function is identical to scratch allocator
ALLO_FUNC size_t stack_allocator_t::round_up_to_valid_buffersize(
    size_t needed_bytes) const noexcept
{
    // NOTE: the fact that we always grow the buffer by two is SUPER
    // hardcoded here since we use log2. you'll have to use some fancy
    // log rules to get log base 1.5 or some other multiplier
    return static_cast<size_t>(std::round(
        std::pow(2.0,
                 std::floor(std::log2(static_cast<double>(needed_bytes) /
                                      static_cast<double>(m.original_size))) +
                     1) *
        static_cast<double>(m.original_size)));
}

// NOTE: this function is identical to scratch allocator
ALLO_FUNC allocation_status_t stack_allocator_t::try_make_space_for_at_least(
    size_t bytes, uint8_t alignment_exponent) noexcept
{
    if (m.parent.is_null())
        return AllocationStatusCode::OOM;

    // if the parent is a heap allocator, we can remap
    if (m.parent.is_heap()) {
        size_t aligned_data =
            ((reinterpret_cast<size_t>(m.top) >> alignment_exponent) + 1)
            << alignment_exponent;

        size_t new_size = round_up_to_valid_buffersize(
            m.memory.size() + bytes +
            (aligned_data - reinterpret_cast<size_t>(m.top)));
        assert(new_size > m.memory.size());
        assert(new_size % m.original_size == 0);
        assert(is_power_of_two(new_size / m.original_size));
        auto res =
            m.parent.get_heap_unchecked().remap_bytes(m.memory, 0, new_size, 0);
        if (res.okay()) {
#ifndef NDEBUG
            if (m.blocks) {
                assert(m.blocks->end().has_value());
                assert(m.blocks->end().value() == m.memory);
            }
#endif
            auto newmem = res.release();
            if (m.blocks) {
                // we already asserted t hat there is an end and it is our
                // current memory block, so just overwrite it with the
                // now-remapped block
                m.blocks->end_unchecked() = newmem;
            }
            m.memory = newmem;
            return AllocationStatusCode::Okay;
        }
    }

    // if we're not a heap, or if heap failed the remap, then do this.
    if (!m.blocks) {
        auto make_segmented_stack_at =
            [this](void* location) -> allocation_status_t {
            if (m.parent.is_heap()) {
                auto new_blocks_stack = segmented_stack_t<bytes_t>::make_owning(
                    m.parent.get_heap_unchecked(), blocks_stack_initial_items);

                if (!new_blocks_stack.okay())
                    return new_blocks_stack.err();

                new (location)
                    segmented_stack_t<bytes_t>(new_blocks_stack.release());
            } else {
                assert(m.parent.is_basic());
                auto new_blocks_stack = segmented_stack_t<bytes_t>::make(
                    m.parent.get_basic_unchecked(), blocks_stack_initial_items);

                if (!new_blocks_stack.okay())
                    return new_blocks_stack.err();

                new (location)
                    segmented_stack_t<bytes_t>(new_blocks_stack.release());
            }
            return AllocationStatusCode::Okay;
        };

        // first: try to allocate the blocks stack inside of our existing memory
        void* top = m.top;
        size_t space = m.memory.end().ptr() - m.top;
        if (std::align(alignof(segmented_stack_t<bytes_t>),
                       sizeof(segmented_stack_t<bytes_t>), top, space)) {

            auto status = make_segmented_stack_at(top);
            if (!status.okay()) [[unlikely]]
                return status.err();

            m.blocks = static_cast<segmented_stack_t<bytes_t>*>(top);

            const auto res = m.blocks->try_push(m.memory);
            assert(res.okay());
            m.top = reinterpret_cast<uint8_t*>(m.blocks + 1);
        } else {
            // allocate a new block with space for both the blocks stack and
            // the new space as well
            const size_t necessary_size = round_up_to_valid_buffersize(
                sizeof(segmented_stack_t<bytes_t>) + bytes +
                // add generous space for alignment of stack
                (alignof(segmented_stack_t<bytes_t>) * 2) +
                // add generous space for alignment of thing we're making space
                // for
                ((1UL << alignment_exponent) * 2UL));
            auto maybe_newblock = m.parent.cast_to_basic().alloc_bytes(
                necessary_size > m.memory.size() ? necessary_size
                                                 : m.memory.size(),
                detail::nearest_alignment_exponent(
                    alignof(segmented_stack_t<bytes_t>)),
                0);

            if (!maybe_newblock.okay()) [[unlikely]]
                return maybe_newblock.err();

            bytes_t newblock = maybe_newblock.release();
            auto status = make_segmented_stack_at(newblock.data());
            if (!status.okay()) [[unlikely]] {
                if (m.parent.is_heap()) {
                    allo::free(m.parent.get_heap_unchecked(), newblock);
                }
                return status.err();
            }

            m.blocks =
                reinterpret_cast<segmented_stack_t<bytes_t>*>(newblock.data());

            auto res = m.blocks->try_push(m.memory);
            assert(res.okay());
            res = m.blocks->try_push(newblock);
            assert(res.okay());
            m.memory = newblock;
            m.top = reinterpret_cast<uint8_t*>(m.blocks + 1);
            return AllocationStatusCode::Okay;
        }
    }

    const size_t new_buffersize = round_up_to_valid_buffersize(
        bytes + ((1UL << alignment_exponent) * 2UL));

    auto maybe_newblock = m.parent.cast_to_basic().alloc_bytes(
        new_buffersize > m.memory.size() ? new_buffersize : m.memory.size(), 3,
        0);

    if (!maybe_newblock.okay()) [[unlikely]]
        return maybe_newblock.err();

    const auto newblock = maybe_newblock.release();

    const auto res = m.blocks->try_push(newblock);
    if (res.okay()) [[unlikely]]
        return res.err();
    m.memory = newblock;
    m.top = newblock.data();

    return AllocationStatusCode::Okay;
}

ALLO_FUNC allocation_result_t stack_allocator_t::alloc_bytes(
    const size_t bytes, const uint8_t alignment_exponent,
    const size_t typehash) noexcept
{
    // TODO: all the alignment stuff in this function could probably become some
    // bithacks, optimize this at some point
    const auto max = [](size_t a, size_t b) { return a > b ? a : b; };
    const size_t actual_alignment =
        max(1 << alignment_exponent, alignof(previous_state_t));

    uint8_t* const oldtop = m.top;
#ifndef NDEBUG
    bool allocated_new_buffer_for_prevstate = false;
    size_t times_retry = 0;
#endif
    previous_state_t* prevstate;
    void* item;

#ifndef NDEBUG
    const size_t original_blocksize = m.blocks ? m.blocks->size() : 0;
#endif

    const auto alloc_new_buffer = [actual_alignment, bytes, &max, this]() {
        size_t extra_space =
            max(actual_alignment, sizeof(previous_state_t)) * 2;
        return try_make_space_for_at_least(
            extra_space + bytes,
            detail::nearest_alignment_exponent(actual_alignment));
    };

    while (1) {
        prevstate = static_cast<previous_state_t*>(
            raw_alloc(alignof(previous_state_t), sizeof(previous_state_t)));

        if (!prevstate) {
            assert(times_retry == 0);
            assert(!allocated_new_buffer_for_prevstate);
            const allocation_status_t buffer_status = alloc_new_buffer();
            if (!buffer_status.okay()) [[unlikely]] {
                m.top = oldtop;
                return buffer_status.err();
            }
#ifndef NDEBUG
            allocated_new_buffer_for_prevstate = true;
            ++times_retry;
#endif
            // retry, this time with the new buffer allocated
            continue;
        }

        item = raw_alloc(actual_alignment, bytes);
#ifndef NDEBUG
        if (allocated_new_buffer_for_prevstate) {
            // NOTE: we cannot recover from this: only one allocation can occur
            // and it *must* fit both items
            assert(item);
        }
#endif
        if (!item) {
            assert(times_retry == 0);
            assert(!allocated_new_buffer_for_prevstate);
            const allocation_status_t buffer_status = alloc_new_buffer();
            if (!buffer_status.okay()) [[unlikely]] {
                m.top = oldtop;
                return buffer_status.err();
            }
#ifndef NDEBUG
            ++times_retry;
#endif
            // NOTE: here we are allowing the prevstate we allocated at the end
            // of the old buffer to just be left there: we never did anything
            // with it. we'll have to allocate a new one now, on the next retry
            continue;
        }

        // if there is space between the two for alignment, move the prevstate
        // up to be as close to the item as possible.
        if (actual_alignment > alignof(previous_state_t)) {
            assert(zl::memcontains_one(m.memory, prevstate));
            assert(zl::memcontains_one(m.memory, (uint8_t*)item));
            while (prevstate + 2 <= item) {
                ++prevstate;
            }
        }
        *prevstate = {
#ifndef ALLO_DISABLE_TYPEINFO
            .type_hashcode = m.last_type_hashcode,
#endif
            .stack_top = oldtop,
        };
        // all good, allocated both successfully and the alignment is less so
        // the previous state is guaranteed to be right before the item
        break;
    }
    assert(zl::memcontains_one(m.memory, (uint8_t*)item));
    assert(zl::memcontains_one(m.memory, prevstate));
    // in debug mode, make sure the prevstate's stack top is in the previous
    // block, if we had to make a new block
#ifndef NDEBUG
    if (m.blocks && original_blocksize > m.blocks->size()) {
        assert(m.blocks->end_unchecked() == m.memory);
        m.blocks->pop();
        assert(m.blocks->end());
        assert(zl::memcontains_one(m.blocks->end_unchecked(),
                                   prevstate->stack_top) ||
               m.blocks->end_unchecked().end().ptr() == prevstate->stack_top);
        assert(m.blocks->try_push(m.memory).okay());
    }
#endif

#ifndef ALLO_DISABLE_TYPEINFO
    m.last_type_hashcode = typehash;
#endif

    return zl::raw_slice<uint8_t>(*reinterpret_cast<uint8_t*>(item), bytes);
}

ALLO_FUNC void* stack_allocator_t::raw_alloc(size_t align,
                                             size_t typesize) noexcept
{
    // these will get modified in place by std::align
    void* new_available_start = m.top;
    size_t new_size = bytes_remaining();
    if (std::align(align, typesize, new_available_start, new_size)) {
        assert(new_available_start < (m.memory.end().ptr() - typesize));
        m.top = reinterpret_cast<uint8_t*>(new_available_start) + typesize;
        assert(zl::memcontains_one(m.memory, m.top) ||
               m.memory.end().ptr() == m.top);
        return new_available_start;
    }
    return nullptr;
}

ALLO_FUNC allocation_status_t
stack_allocator_t::free_status(bytes_t mem, size_t typehash) const noexcept
{
    return free_common(mem, typehash).err();
}

ALLO_FUNC zl::res<stack_allocator_t::previous_state_t&, AllocationStatusCode>
stack_allocator_t::free_common(bytes_t mem, size_t typehash) const noexcept
{
#ifndef ALLO_DISABLE_TYPEINFO
    assert(typehash == m.last_type_hashcode);
    if (typehash != m.last_type_hashcode)
        return AllocationStatusCode::InvalidType;
#endif
    assert(m.top == mem.end().ptr());
    if (m.top != mem.end().ptr()) [[unlikely]] {
        return AllocationStatusCode::MemoryInvalid;
    }

    void* bufbegin = mem.data();
    size_t dummysize = sizeof(previous_state_t) * 10;
    auto* prevstate = static_cast<previous_state_t*>(
        std::align(alignof(previous_state_t), sizeof(previous_state_t),
                   bufbegin, dummysize));
    assert(prevstate);
    while (reinterpret_cast<uint8_t*>(prevstate + 1) > mem.data()) {
        --prevstate;
    }
#ifndef NDEBUG
    if (m.blocks) {
        m.blocks->pop();
        assert(zl::memcontains_one(m.memory, prevstate->stack_top) ||
               zl::memcontains_one(m.blocks->end_unchecked(),
                                   prevstate->stack_top) ||
               m.blocks->end_unchecked().end().ptr() == prevstate->stack_top);
        assert(m.blocks->try_push(m.memory).okay());
    } else {
        assert(zl::memcontains_one(m.memory, prevstate->stack_top));
    }
#endif

    return *prevstate;
}

ALLO_FUNC allocation_status_t
stack_allocator_t::free_bytes(bytes_t mem, size_t typehash) noexcept
{
    auto maybe_prevstate = free_common(mem, typehash);
    if (!maybe_prevstate.okay()) [[unlikely]]
        return maybe_prevstate.err();

    previous_state_t& prevstate = maybe_prevstate.release();
    if (m.blocks && !zl::memcontains_one(m.memory, prevstate.stack_top)) {
        // stack top is not in our current block...
        if (m.parent.is_heap()) {
            m.parent.get_heap_unchecked().free_bytes(m.memory, 0);
        }

        // NOTE: leaking memory here if parent allocator is stack
        m.blocks->pop();
        m.memory = m.blocks->end_unchecked();
    }
    m.top = prevstate.stack_top;
    m.last_type_hashcode = prevstate.type_hashcode;
    assert(zl::memcontains_one(m.memory, m.top) ||
           m.memory.end().ptr() == m.top);
    return AllocationStatusCode::Okay;
}

ALLO_FUNC allocation_result_t
stack_allocator_t::remap_bytes(bytes_t mem, size_t old_typehash,
                               size_t new_size, size_t new_typehash) noexcept
{
#ifndef ALLO_DISABLE_TYPEINFO
    if (old_typehash != m.last_type_hashcode)
        return AllocationStatusCode::InvalidType;
#endif
    assert(m.top == mem.end().ptr());
    if (m.top != mem.end().ptr()) [[unlikely]] {
        return AllocationStatusCode::MemoryInvalid;
    }

    const size_t byte_index_of_original_mem = mem.data() - m.memory.data();
    // the index of the next byte where we can allocate stuff after this
    const size_t byte_index_of_new_begin =
        byte_index_of_original_mem + new_size;
    if (byte_index_of_new_begin > m.memory.size()) [[unlikely]] {
        return AllocationStatusCode::OOM;
    }

    // modify our invariants
    m.top = m.memory.data() + byte_index_of_new_begin;
#ifndef ALLO_DISABLE_TYPEINFO
    m.last_type_hashcode = new_typehash;
#endif
    assert(zl::memcontains_one(m.memory, m.top) ||
           m.memory.end().ptr() == m.top);
    return bytes_t(m.memory, byte_index_of_original_mem,
                   byte_index_of_new_begin);
}

ALLO_FUNC stack_allocator_t
stack_allocator_t::make_inner(bytes_t memory, any_allocator_t parent) noexcept
{
#ifndef NDEBUG
    if (parent.is_heap()) {
        assert(parent.get_heap_unchecked().free_status(memory, 0).okay());
    }
#endif
    return M{
        .memory = memory,
        .top = memory.data(),
        .original_size = memory.size(),
        .parent = parent,
    };
}

// NOTE: this function is identical to scratch allocator
ALLO_FUNC allocation_status_t stack_allocator_t::register_destruction_callback(
    destruction_callback_t callback, void* user_data) noexcept
{
    if (!callback) {
        assert(callback);
        return AllocationStatusCode::InvalidArgument;
    }
    auto res = allo::alloc_one<destruction_callback_entry_t>(*this);
    if (!res.okay())
        return res.err();
    destruction_callback_entry_t& newentry = res.release();
    newentry = destruction_callback_entry_t{
        .callback = callback,
        .user_data = user_data,
        .prev = m.last_callback,
    };
    m.last_callback = &newentry;
    return AllocationStatusCode::Okay;
}
} // namespace allo
