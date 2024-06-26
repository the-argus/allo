#pragma once
#include <memory>
#include <ziglike/stdmem.h>
#ifndef ALLO_HEADER_ONLY
#ifndef ALLO_OVERRIDE_IMPL_INCLUSION_GUARD
#error \
    "Attempt to include allo/impl header file but header-only mode is not enabled."
#endif
#endif

#include "allo/detail/asserts.h"
#include "allo/scratch_allocator.h"

#ifdef ALLO_HEADER_ONLY
#ifndef ALLO_FUNC
#define ALLO_FUNC inline
#endif
#else
#define ALLO_FUNC
#endif

namespace allo {

#ifndef NDEBUG
inline constexpr bool is_power_of_two(size_t n)
{
    return n != 0 && (n & (n - 1)) == 0;
}
#endif

// round up to be an even integer exponential of 2, times the original
// size.
ALLO_FUNC size_t scratch_allocator_t::round_up_to_valid_buffersize(
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

ALLO_FUNC allocation_status_t scratch_allocator_t::try_make_space_for_at_least(
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
        ALLO_INTERNAL_ASSERT(new_size > m.memory.size());
        ALLO_INTERNAL_ASSERT(new_size % m.original_size == 0);
        ALLO_INTERNAL_ASSERT(is_power_of_two(new_size / m.original_size));
        auto res =
            m.parent.get_heap_unchecked().remap_bytes(m.memory, 0, new_size, 0);
        if (res.okay()) {
#ifndef NDEBUG
            if (m.blocks) {
                ALLO_INTERNAL_ASSERT(m.blocks->end().has_value());
                ALLO_INTERNAL_ASSERT(m.blocks->end().value() == m.memory);
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
                ALLO_INTERNAL_ASSERT(m.parent.is_basic());
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
            ALLO_INTERNAL_ASSERT(res.okay());
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
            // these should work because we reserved 2 spots in the constructor
            ALLO_INTERNAL_ASSERT(res.okay());
            res = m.blocks->try_push(newblock);
            ALLO_INTERNAL_ASSERT(res.okay());
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

ALLO_FUNC allocation_result_t scratch_allocator_t::alloc_bytes(
    size_t bytes, uint8_t alignment_exponent, size_t) noexcept
{
    auto tryalloc = [this](size_t bytes,
                           uint8_t alignment_exponent) -> allocation_result_t {
        void* new_top = m.top;
        size_t remaining = m.memory.end().ptr() - m.top;
        if (std::align(1UL << alignment_exponent, bytes, new_top, remaining)) {
            ALLO_INTERNAL_ASSERT(remaining >= bytes);
            auto result = zl::raw_slice(*static_cast<uint8_t*>(new_top), bytes);
            m.top = static_cast<uint8_t*>(new_top) + bytes;
            return result;
        }
        return AllocationStatusCode::OOM;
    };
    auto res = tryalloc(bytes, alignment_exponent);
    if (!res.okay()) {
        auto status = try_make_space_for_at_least(bytes, alignment_exponent);
        if (!status.okay()) [[unlikely]] {
            // NOTE: should we ignore errors here and just still try to alloc?
            return status.err();
        }
        return tryalloc(bytes, alignment_exponent);
    }
    return res;
}

ALLO_FUNC allocation_status_t
scratch_allocator_t::register_destruction_callback(
    destruction_callback_t callback, void* user_data) noexcept
{
    return detail::register_destruction_callback_with_only_one_item_per_array<
        scratch_allocator_t>(this, m.last_callback, callback, user_data);
}

ALLO_FUNC scratch_allocator_t
scratch_allocator_t::make_inner(bytes_t memory, any_allocator_t parent) noexcept
{
    return M{
        .memory = memory,
        .top = memory.data(),
        .original_size = memory.size(),
        .parent = parent,
    };
}

ALLO_FUNC scratch_allocator_t::~scratch_allocator_t() noexcept
{
    detail::call_all_destruction_callbacks(m.last_callback);

    // free if we are owning
    if (!m.parent.is_heap())
        return;

    if (m.blocks) {
        while (auto iter = m.blocks->end()) {
            m.parent.get_heap_unchecked().free_bytes(iter.value(), 0);
            m.blocks->pop();
        }
        m.blocks->~segmented_stack_t<bytes_t>();
        free_one(m.parent.get_heap_unchecked(), *m.blocks);
    } else {
        m.parent.get_heap_unchecked().free_bytes(m.memory, 0);
    }
}

ALLO_FUNC
scratch_allocator_t::scratch_allocator_t(scratch_allocator_t&& other) noexcept
    : m(other.m)
{
    m_type = other.m_type;
    other.m.parent = any_allocator_t();
    other.m.last_callback = nullptr;
}
} // namespace allo
