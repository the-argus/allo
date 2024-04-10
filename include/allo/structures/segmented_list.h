#pragma once
// we have to make sure that impls arent included in header-only mode, because
// otherwise we'll get circular header dependencies when the impl for
// detail/abstracts includes stack allocator which includes this
#ifdef ALLO_HEADER_ONLY
#define ALLO_HEADER_ONLY_AVOID
#undef ALLO_HEADER_ONLY
#endif
#include "allo/typed_allocation.h"
#include "allo/typed_freeing.h"
#include "allo/typed_reallocation.h"
#ifdef ALLO_HEADER_ONLY_AVOID
#undef ALLO_HEADER_ONLY_AVOID
#define ALLO_HEADER_ONLY
#endif

#include <ziglike/joined_slice.h>
#include <ziglike/slice.h>

namespace allo {
template <typename T> class segmented_list_t
{
  public:
    using type = T;

    enum class StatusCode : uint8_t
    {
        Okay,
        ResultReleased,
        IndexOutOfRange,
        OOM,
    };

    using status_t = zl::status<StatusCode>;

    [[nodiscard]] static zl::res<segmented_list_t, AllocationStatusCode>
    make(detail::abstract_heap_allocator_t &parent_allocator,
         size_t initial_items, zl::opt<size_t> items_per_segment = {}) noexcept;

    /// Version of make where the list will not free its contents upon
    /// destruction, but it does not require an allocator which frees. Useful
    /// for creating lists within a stack or scratch allocator.
    [[nodiscard]] static zl::res<segmented_list_t, AllocationStatusCode>
    make_unowned(detail::abstract_allocator_t &parent_allocator,
                 size_t initial_items,
                 zl::opt<size_t> items_per_segment = {}) noexcept;

    segmented_list_t() = delete;
    segmented_list_t(const segmented_list_t &) = delete;
    segmented_list_t &operator=(const segmented_list_t &) = delete;
    segmented_list_t(segmented_list_t &&) noexcept = default;
    segmented_list_t &operator=(segmented_list_t &&) noexcept = default;

    [[nodiscard]] constexpr zl::joined_slice<const T> items() const noexcept;

    [[nodiscard]] constexpr zl::joined_slice<T> items() noexcept;

    [[nodiscard]] constexpr size_t size() const noexcept;

    [[nodiscard]] constexpr size_t capacity() const noexcept;

    template <typename... Args>
    [[nodiscard]] allocation_status_t try_insert_at(size_t index,
                                                    Args &&...args) noexcept
    {
    }

    [[nodiscard]] status_t try_remove_at(size_t index) noexcept {}

    template <typename... Args>
    [[nodiscard]] allocation_status_t try_append(Args &&...args) noexcept
    {
    }

    [[nodiscard]] zl::opt<T &> try_get_at(size_t index) noexcept {}

    [[nodiscard]] zl::opt<const T &> try_get_at(size_t index) const noexcept {}

    void remove_at_unchecked(size_t index) noexcept {}

    [[nodiscard]] T &get_at_unchecked(size_t index) noexcept {}

    [[nodiscard]] const T &get_at_unchecked(size_t index) const noexcept {}

    inline ~segmented_list_t() noexcept
    {
        if (m.is_heap) {
            for (auto &slice : m.items) {
                zl::slice<T> fullslice =
                    zl::raw_slice(*slice.data(), m.items_per_segment);
                allo::free(*m.parent.heap, fullslice);
            }
        }
    }

  private:
    allocation_status_t try_alloc_space_for(size_t items) noexcept;

    struct M
    {
        zl::slice<zl::slice<T>> items;
        size_t items_per_segment;
        size_t capacity;
        union allocator_variant_u
        {
            detail::abstract_heap_allocator_t *heap;
            detail::abstract_allocator_t *basic;
        } parent;
        bool is_heap;
    } m;
};
} // namespace allo
