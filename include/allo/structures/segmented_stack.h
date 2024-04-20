#pragma once
#ifdef ALLO_HEADER_ONLY
#define ALLO_HEADER_ONLY_AVOID
#undef ALLO_HEADER_ONLY
#endif
#include "allo/detail/calculate_segment_size.h"
#include "allo/structures/any_allocator.h"
#include "allo/structures/uninitialized_array.h"
#include "allo/typed_allocation.h"
#include "allo/typed_freeing.h"
#include <cmath>
#include <ziglike/opt.h>
#ifdef ALLO_HEADER_ONLY_AVOID
#undef ALLO_HEADER_ONLY_AVOID
#define ALLO_HEADER_ONLY
#endif

namespace allo {
template <typename T> class segmented_stack_t
{
  private:
    struct Segment;
    struct SegmentEndcap
    {
        Segment *prev;
        Segment *next;
    };
    using segment_info = detail::segment_size_with_endcap<T, SegmentEndcap>;
    static constexpr size_t segment_size = segment_info::value;
    static constexpr size_t items_per_segment = segment_info::number_of_items;
    struct Segment
    {
        uninitialized_array_t<T, items_per_segment> items;
        SegmentEndcap endcap;
    };
    static_assert(sizeof(Segment) == segment_size);

  public:
    segmented_stack_t() = delete;
    segmented_stack_t(const segmented_stack_t &) = delete;
    segmented_stack_t &operator=(const segmented_stack_t &) = delete;
    segmented_stack_t(segmented_stack_t &&) noexcept = default;
    segmented_stack_t &operator=(segmented_stack_t &&) noexcept = default;

    /// Make a segmented stack with a generic allocator. A segmented
    /// stack never needs to reallocate, so this can be done
    /// without causing memory fragmentation (unless you use the list to
    /// allocate many things and then remove the vast majority of them, since
    /// the segmented stack will not shrink its allocation when not
    /// in use)
    [[nodiscard]] static zl::res<segmented_stack_t, AllocationStatusCode>
    make(detail::abstract_allocator_t &parent_allocator,
         size_t initial_items) noexcept;

    /// Make a segmented stack with a heap allocator, in which case
    /// the stack will free its contents upon destruction.
    [[nodiscard]] static zl::res<segmented_stack_t, AllocationStatusCode>
    make_owned(detail::abstract_heap_allocator_t &parent_allocator,
               size_t initial_items) noexcept;

    [[nodiscard]] constexpr size_t size() const noexcept;

    [[nodiscard]] constexpr zl::opt<T &> end() noexcept;

    constexpr void pop() noexcept;

    template <typename Callable>
    inline constexpr void for_each(Callable &&callable) noexcept
    {
        static_assert(std::is_invocable_r_v<void, Callable, T &>,
                      "The given function either does not return void or "
                      "cannot be called with just a T&.");
        Segment *iter = &m.head;
        size_t index = 0;
        while (index <= m.index_of_segment_containing_end) {
            zl::slice<T> items_in_this_segment =
                index == m.index_of_segment_containing_end
                    ? zl::slice<T>(iter->items, 0,
                                   m.items_in_segment_containing_end)
                    : zl::slice<T>(iter->items);

            for (T &item : items_in_this_segment) {
                callable(item);
            }

            ++index;
            iter = iter->endcap.next;
        }
    }

    template <typename... Args>
    [[nodiscard]] inline allocation_status_t try_push(Args &&...args) noexcept
    {
        if (m.items_in_segment_containing_end == items_per_segment) {
            // if we already have a segment allocated after this one, just use
            // that. if we dont have another segment after the end, allocate a
            // new one
            if (m.segment_containing_end->endcap.next) {
                m.segment_containing_end =
                    m.segment_containing_end->endcap.next;
            } else {
                auto maybe_segment =
                    alloc_one<Segment, detail::abstract_allocator_t,
                              detail::cache_line_size>(
                        m.parent.cast_to_basic());
                if (!maybe_segment.okay())
                    return maybe_segment.err();
                Segment &new_segment = maybe_segment.release();
                // intialize the previous segment to point to the new one
                m.segment_containing_end->endcap.next = &new_segment;
                // initialize the new segment to point to the old
                new_segment.endcap = SegmentEndcap{
                    .prev = m.segment_containing_end,
                    .next = nullptr,
                };
                // update to new segment
                m.segment_containing_end = &new_segment;
            }
            ++m.index_of_segment_containing_end;
            m.items_in_segment_containing_end = 0;
        }
        // assert that there is space in the current segment for another thing
        assert(m.items_in_segment_containing_end < items_per_segment);
        assert(m.items_in_segment_containing_end <
               m.segment_containing_end->items.size());

        static_assert(std::is_nothrow_constructible_v<T, Args...>,
                      "Constructor that is being called by try_push is not "
                      "nothrow, or doesn't exist.");

        new (m.segment_containing_end->items.data() +
             m.items_in_segment_containing_end) T(std::forward<Args>(args)...);

        ++m.items_in_segment_containing_end;
        return AllocationStatusCode::Okay;
    }

    [[nodiscard]] constexpr T &end_unchecked() noexcept;

    inline ~segmented_stack_t() noexcept
    {
        if (m.parent.is_heap()) {
            Segment *iter = &m.head;
            while (iter) {
                Segment *new_iter = iter->endcap.next;
                allo::free_one(m.parent.get_heap_unchecked(), *iter);
                iter = new_iter;
            }
        }
    }

  private:
    struct M
    {
        // the first segment in the linked list of segments
        Segment &head;
        // the segment which contains the topmost item (never null)
        Segment *segment_containing_end;
        // the index of "segment_containing_end" in the singly linked list of
        // segments
        size_t index_of_segment_containing_end;
        // the number of things in the segment which should have the end. if
        // this is zero, there are no items in the stack and
        // index_of_segment_containing_end should also be 0
        size_t items_in_segment_containing_end;
        any_allocator_t parent;
    } m;

  public:
    inline constexpr segmented_stack_t(M members) noexcept : m(members) {}
};

template <typename T>
auto segmented_stack_t<T>::make(detail::abstract_allocator_t &parent_allocator,
                                size_t initial_items) noexcept
    -> zl::res<segmented_stack_t, AllocationStatusCode>
{
    const size_t actual_initial_items = initial_items == 0 ? 1 : initial_items;
    const size_t segments_needed =
        std::ceil(static_cast<float>(actual_initial_items) /
                  static_cast<float>(items_per_segment));
    assert(segments_needed != 0);

    // allocate a bunch of segments and link them to each other
    Segment *previous = nullptr;
    Segment *first = nullptr;
    for (size_t i = 0; i < segments_needed; ++i) {
        auto maybe_segment =
            alloc_one<Segment, detail::abstract_allocator_t,
                      detail::cache_line_size>(parent_allocator);
        if (!maybe_segment.okay())
            return maybe_segment.err();
        Segment &segment = maybe_segment.release();
        if (!first)
            first = &segment;
        segment.endcap.next = nullptr;
        segment.endcap.prev = previous;
        if (previous) {
            previous->endcap.next = &segment;
        }
        previous = &segment;
    }
    assert(first);

    return segmented_stack_t(M{
        .head = *first,
        .segment_containing_end = first,
        .index_of_segment_containing_end = 0,
        .items_in_segment_containing_end = 0,
        .parent = parent_allocator,
    });
}

template <typename T>
auto segmented_stack_t<T>::make_owned(
    detail::abstract_heap_allocator_t &parent_allocator,
    size_t initial_items) noexcept
    -> zl::res<segmented_stack_t, AllocationStatusCode>
{
    const size_t actual_initial_items = initial_items == 0 ? 1 : initial_items;
    const size_t segments_needed =
        std::ceil(static_cast<float>(actual_initial_items) /
                  static_cast<float>(items_per_segment));
    assert(segments_needed != 0);

    // allocate a bunch of segments and link them to each other
    Segment *previous = nullptr;
    Segment *first = nullptr;
    for (size_t i = 0; i < segments_needed; ++i) {
        auto maybe_segment =
            alloc_one<Segment, detail::abstract_heap_allocator_t,
                      detail::cache_line_size>(parent_allocator);
        if (!maybe_segment.okay()) {
            Segment *iter = first;
            // clean up since we can free, this is a heap allocator
            while (iter) {
                free_one(parent_allocator, *iter);
                iter = iter->endcap.next;
            }
            return maybe_segment.err();
        }
        Segment &segment = maybe_segment.release();
        if (!first)
            first = &segment;
        segment.endcap.next = nullptr;
        segment.endcap.prev = previous;
        if (previous) {
            previous->endcap.next = &segment;
        }
        previous = &segment;
    }
    assert(first);

    return segmented_stack_t(M{
        .head = *first,
        .segment_containing_end = first,
        .index_of_segment_containing_end = 0,
        .items_in_segment_containing_end = 0,
        .parent = parent_allocator,
    });
}

template <typename T>
constexpr size_t segmented_stack_t<T>::size() const noexcept
{
    return (items_per_segment * m.index_of_segment_containing_end) +
           m.items_in_segment_containing_end;
}

template <typename T>
constexpr zl::opt<T &> segmented_stack_t<T>::end() noexcept
{
    if (m.items_in_segment_containing_end == 0) {
        return {};
    }
    assert(m.items_in_segment_containing_end <= items_per_segment);
    return end_unchecked();
}

template <typename T> constexpr void segmented_stack_t<T>::pop() noexcept
{
    if (m.items_in_segment_containing_end == 0) {
        return;
    }
    static_assert(
        std::is_nothrow_destructible_v<T>,
        "Cannot pop off a stack whose contents are not nothrow destructible.");
    // destroy the end item
    end_unchecked().~T();
    // move our offset within the segment back one
    --m.items_in_segment_containing_end;
    // if that also moves us by a segment, traverse the list backwards one
    if (m.items_in_segment_containing_end == 0 &&
        m.index_of_segment_containing_end != 0) {
        --m.index_of_segment_containing_end;
        assert(m.segment_containing_end->endcap.prev);
        m.segment_containing_end = m.segment_containing_end->endcap.prev;
        // if previous one exists, its definitely full
        m.items_in_segment_containing_end = items_per_segment;
    }
}

template <typename T>
constexpr T &segmented_stack_t<T>::end_unchecked() noexcept
{
    assert(m.items_in_segment_containing_end != 0);
    return m.segment_containing_end->items
        .data()[m.items_in_segment_containing_end - 1];
}

} // namespace allo
