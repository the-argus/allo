#pragma once
#include <cmath>
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

#include <ziglike/slice.h>

namespace allo {
template <typename T> class list_t
{
  public:
    using type = T;

    static constexpr double realloc_ratio = 1.5;

    enum class StatusCode : uint8_t
    {
        Okay,
        ResultReleased,
        IndexOutOfRange,
        OOM,
    };

    using status_t = zl::status<StatusCode>;

    list_t() = delete;
    list_t(const list_t &) = delete;
    list_t &operator=(const list_t &) = delete;
    list_t(list_t &&) noexcept = default;
    list_t &operator=(list_t &&) noexcept = default;

    [[nodiscard]] static zl::res<list_t, AllocationStatusCode>
    make(zl::slice<uint8_t> memory) noexcept;

    [[nodiscard]] static zl::res<list_t, AllocationStatusCode>
    make_owned(detail::abstract_heap_allocator_t &parent_allocator,
               size_t initial_items) noexcept;

    [[nodiscard]] constexpr zl::slice<const T> items() const noexcept;

    [[nodiscard]] constexpr zl::slice<T> items() noexcept;

    [[nodiscard]] constexpr size_t capacity() const noexcept;

    template <typename... Args>
    [[nodiscard]] allocation_status_t try_insert_at(size_t index,
                                                    Args &&...args) noexcept
    {
        const bool needs_realloc = m.memory.size() <= m.size;
        // NOTE: reallocation here is very simple, because you can only insert
        // one item at a time.
        if (needs_realloc) {
            if (m.parent) {
                const auto newsize = static_cast<size_t>(std::ceil(
                    static_cast<double>(m.memory.size()) * realloc_ratio));

                auto realloc_result =
                    allo::realloc(m.parent.value(), m.memory, newsize);
                if (!realloc_result.okay())
                    return realloc_result.err();

                m.memory = realloc_result.release();
            } else {
                return AllocationStatusCode::OOM;
            }
        }
        ++m.size;
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

    inline ~list_t() noexcept
    {
        if (m.parent) {
            allo::free(m.parent.value(), m.memory);
        }
    }

  private:
    struct M
    {
        zl::opt<detail::abstract_heap_allocator_t &> parent;
        zl::slice<T> memory;
        size_t size;
    } m;
};

template <typename T>
[[nodiscard]] constexpr zl::slice<const T> list_t<T>::items() const noexcept
{
    return zl::slice<const T>(m.memory, 0, m.size);
}

template <typename T>
[[nodiscard]] constexpr zl::slice<T> list_t<T>::items() noexcept
{
    return zl::slice<T>(m.memory, 0, m.size);
}

template <typename T>
[[nodiscard]] constexpr size_t list_t<T>::capacity() const noexcept
{
    return m.memory.size();
}

} // namespace allo
