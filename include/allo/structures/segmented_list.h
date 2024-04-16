#pragma once
#include <cmath>
#include <type_traits>
#ifdef ALLO_HEADER_ONLY
#define ALLO_HEADER_ONLY_AVOID
#undef ALLO_HEADER_ONLY
#endif
#include "allo/detail/calculate_segment_size.h"
#include "allo/structures/any_allocator.h"
#include "allo/structures/segmented_stack.h"
#include "allo/typed_allocation.h"
#include "allo/typed_freeing.h"
#ifdef ALLO_HEADER_ONLY_AVOID
#undef ALLO_HEADER_ONLY_AVOID
#define ALLO_HEADER_ONLY
#endif

#include <ziglike/defer.h>
#include <ziglike/joined_slice.h>

namespace allo {
template <typename T> class segmented_list_t
{
  public:
    static_assert(std::is_nothrow_destructible_v<T>,
                  "Cannot instantiate a list whose contents are not nothrow "
                  "destructible.");

    using type = T;

    // calculate the number of bytes that will be allocated for each segment of
    // this structure
    static constexpr size_t segment_size = detail::segment_size<T>::value;
    static constexpr size_t items_per_segment = segment_size / sizeof(T);
    static_assert(
        segment_size % sizeof(T) == 0,
        "Items of given type do not fit evenly into a segment as expected.");

    enum class StatusCode : uint8_t
    {
        Okay,
        ResultReleased,
        IndexOutOfRange,
        AllocatorError,
        OOM,
    };

    using status_t = zl::status<StatusCode>;

    segmented_list_t() = delete;
    segmented_list_t(const segmented_list_t &) = delete;
    segmented_list_t &operator=(const segmented_list_t &) = delete;
    segmented_list_t(segmented_list_t &&) noexcept = default;
    segmented_list_t &operator=(segmented_list_t &&) noexcept = default;

    [[nodiscard]] static zl::res<segmented_list_t, AllocationStatusCode>
    make(detail::abstract_allocator_t &parent_allocator,
         size_t initial_items) noexcept;

    [[nodiscard]] static zl::res<segmented_list_t, AllocationStatusCode>
    make_owned(detail::abstract_heap_allocator_t &parent_allocator,
               size_t initial_items) noexcept;

    [[nodiscard]] constexpr zl::joined_slice<const T> items() const noexcept;

    [[nodiscard]] constexpr zl::joined_slice<T> items() noexcept;

    [[nodiscard]] constexpr size_t capacity() const noexcept;

    template <typename... Args>
    [[nodiscard]] status_t try_insert_at(size_t index, Args &&...args) noexcept
    {
    }

    [[nodiscard]] status_t try_remove_at(size_t index) noexcept;

    template <typename... Args>
    [[nodiscard]] status_t try_append(Args &&...args) noexcept
    {
    }

    [[nodiscard]] zl::opt<T &> try_get_at(size_t index) noexcept;

    [[nodiscard]] zl::opt<const T &> try_get_at(size_t index) const noexcept;

    void remove_at_unchecked(size_t index) noexcept;

    [[nodiscard]] T &get_at_unchecked(size_t index) noexcept;

    [[nodiscard]] const T &get_at_unchecked(size_t index) const noexcept;

    ~segmented_list_t() noexcept
    {
        if (m.parent.is_heap()) {
            for (auto slice : m.segments) {
                const auto res = free(m.parent.get_heap_unchecked(), slice);
                assert(res.okay());
            }
            zl::slice<zl::slice<T>> list_memory(m.segments.items(), 0,
                                                m.segments.capacity());
            const auto res = free(m.parent.get_heap_unchecked(), list_memory);
        }
    }

  private:
    struct M
    {
        segmented_stack_t<zl::slice<T>> segments;
        size_t size;
        any_allocator_t parent;
    } m;

    [[nodiscard]] status_t try_alloc_new_segment() noexcept;

  public:
    inline constexpr segmented_list_t(M members) noexcept : m(members) {}
};

template <typename T>
inline auto
segmented_list_t<T>::make(detail::abstract_allocator_t &parent_allocator,
                          size_t initial_items) noexcept
    -> zl::res<segmented_list_t, AllocationStatusCode>
{
    const bool divisible = initial_items % items_per_segment == 0;
    const size_t segments_needed =
        (initial_items / items_per_segment) + (divisible ? 0 : 1);
    auto maybe_memory_for_segments =
        alloc<zl::slice<T>>(parent_allocator, (segments_needed * 2));

    if (!maybe_memory_for_segments.okay())
        return maybe_memory_for_segments.err();

    auto segments = segmented_stack_t<zl::slice<T>>::make(
        maybe_memory_for_segments.release_ref());

    for (size_t i = 0; i < segments_needed; ++i) {
        auto maybe_segment = alloc<T>(parent_allocator, items_per_segment);
        if (!maybe_segment.okay())
            return maybe_segment.err();
        auto res = segments.try_append(maybe_segment.release_ref());
        assert(res.okay());
    }

    const any_allocator_t parent = parent_allocator;
    assert(parent.is_basic());

    return zl::res<segmented_list_t, AllocationStatusCode>{
        std::in_place,
        M{
            .segments = std::move(segments),
            .size = 0,
            .parent = parent,
        },
    };
}

template <typename T>
inline auto segmented_list_t<T>::make_owned(
    detail::abstract_heap_allocator_t &parent_allocator,
    size_t initial_items) noexcept
    -> zl::res<segmented_list_t, AllocationStatusCode>
{
    const bool divisible = initial_items % items_per_segment == 0;
    const size_t segments_needed =
        (initial_items / items_per_segment) + (divisible ? 0 : 1);
    auto maybe_memory_for_segments =
        alloc<zl::slice<T>>(parent_allocator, (segments_needed * 2));

    if (!maybe_memory_for_segments.okay())
        return maybe_memory_for_segments.err();

    zl::defer free_segments_list_mem(
        [&maybe_memory_for_segments, &parent_allocator]() {
            free(parent_allocator, maybe_memory_for_segments);
        });

    auto segments = segmented_stack_t<zl::slice<T>>::make(
        maybe_memory_for_segments.release_ref());

    zl::defer free_segments([&segments, &parent_allocator]() {
        for (auto &slice : segments.items()) {
            free(parent_allocator, slice);
        }
    });

    for (size_t i = 0; i < segments_needed; ++i) {
        auto maybe_segment = alloc<T>(parent_allocator, items_per_segment);
        if (!maybe_segment.okay())
            return maybe_segment.err();
        auto res = segments.try_append(maybe_segment.release_ref());
        assert(res.okay());
    }

    const any_allocator_t parent = parent_allocator;
    assert(parent.is_heap());

    free_segments_list_mem.cancel();
    free_segments.cancel();

    return zl::res<segmented_list_t, AllocationStatusCode>{
        std::in_place,
        M{
            .segments = std::move(segments),
            .size = 0,
            .parent = parent,
        },
    };
}

template <typename T>
constexpr zl::joined_slice<const T> segmented_list_t<T>::items() const noexcept
{
    return m.segments.items();
}

template <typename T>
constexpr zl::joined_slice<T> segmented_list_t<T>::items() noexcept
{
    return m.segments.items();
}

template <typename T>
constexpr size_t segmented_list_t<T>::capacity() const noexcept
{
    return items_per_segment * m.segments.items().size();
}

} // namespace allo
