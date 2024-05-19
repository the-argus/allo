#pragma once
#include <cmath>
#include <type_traits>
#ifdef ALLO_HEADER_ONLY
#define ALLO_HEADER_ONLY_AVOID
#undef ALLO_HEADER_ONLY
#endif
#include "allo/detail/asserts.h"
#include "allo/status.h"
#include "allo/typed_allocation.h"
#include "allo/typed_freeing.h"
#include "allo/typed_reallocation.h"
#ifdef ALLO_HEADER_ONLY_AVOID
#undef ALLO_HEADER_ONLY_AVOID
#define ALLO_HEADER_ONLY
#endif

#include <ziglike/opt.h>
#include <ziglike/slice.h>

namespace allo {
template <typename T> class list_t
{
  public:
    static_assert(std::is_nothrow_destructible_v<T>,
                  "Cannot instantiate a list whose contents are not nothrow "
                  "destructible.");

    using type = T;

    static constexpr double realloc_ratio = 1.5;

    enum class StatusCode : uint8_t
    {
        Okay,
        ResultReleased,
        IndexOutOfRange,
        AllocatorError,
        OOM,
    };

    using status_t = zl::status<StatusCode>;

    list_t() = delete;
    list_t(const list_t&) = delete;
    list_t& operator=(const list_t&) = delete;
    list_t(list_t&&) noexcept = default;
    list_t& operator=(list_t&&) noexcept = default;

    [[nodiscard]] static constexpr list_t make(zl::slice<T> memory) noexcept;

    [[nodiscard]] static zl::res<list_t, AllocationStatusCode>
    make_owning(detail::abstract_heap_allocator_t& parent_allocator,
                size_t initial_items) noexcept;

    [[nodiscard]] constexpr zl::slice<const T> items() const noexcept;

    [[nodiscard]] constexpr zl::slice<T> items() noexcept;

    [[nodiscard]] constexpr size_t capacity() const noexcept;

    template <typename... Args>
    [[nodiscard]] status_t try_insert_at(size_t index, Args&&... args) noexcept
    {
        ALLO_VALID_ARG_ASSERT(index <= m.size);
        if (index > m.size) {
            return StatusCode::IndexOutOfRange;
        }
        auto status = try_realloc_if_needed();
        if (!status.okay())
            return status.err();
        ALLO_INTERNAL_ASSERT(m.memory.size() > m.size);
        // move all items between the end of the buffer and index down one to
        // make room for the new item
        for (int64_t i = m.size; i >= index + 1; --i) {
            new (m.memory.data() + i) T(std::move(m.memory.data()[i - 1]));
        }
        static_assert(
            std::is_nothrow_constructible_v<T, Args...>,
            "T is not nothrow constructible with the given arguments.");
        new (m.memory.data() + index) T(std::forward<Args>(args)...);
        ++m.size;
        return StatusCode::Okay;
    }

    [[nodiscard]] status_t try_remove_at(size_t index) noexcept;

    template <typename... Args>
    [[nodiscard]] status_t try_append(Args&&... args) noexcept
    {
        auto status = try_realloc_if_needed();
        if (!status.okay())
            return status.err();
        static_assert(
            std::is_nothrow_constructible_v<T, Args...>,
            "T is not nothrow constructible with the given arguments.");
        new (items().end().ptr()) T(std::forward<Args>(args)...);
        ++m.size;
        return StatusCode::Okay;
    }

    [[nodiscard]] zl::opt<T&> try_get_at(size_t index) noexcept;

    [[nodiscard]] zl::opt<const T&> try_get_at(size_t index) const noexcept;

    void remove_at_unchecked(size_t index) noexcept;

    [[nodiscard]] T& get_at_unchecked(size_t index) noexcept;

    [[nodiscard]] const T& get_at_unchecked(size_t index) const noexcept;

    inline ~list_t() noexcept
    {
        if (m.parent) {
            allo::free(m.parent.value(), m.memory);
        }
    }

  private:
    struct M
    {
        zl::opt<detail::abstract_heap_allocator_t&> parent;
        zl::slice<T> memory;
        size_t size;
    } m;

    [[nodiscard]] status_t try_realloc_if_needed() noexcept;

  public:
    inline constexpr list_t(M members) noexcept : m(members) {}
};

template <typename T>
constexpr list_t<T> list_t<T>::make(zl::slice<T> memory) noexcept
{
    return M{
        .parent = {},
        .memory = memory,
        .size = 0,
    };
}

template <typename T>
zl::res<list_t<T>, AllocationStatusCode>
list_t<T>::make_owning(detail::abstract_heap_allocator_t& parent_allocator,
                       size_t initial_items) noexcept
{
    auto result = alloc<T>(parent_allocator, initial_items);
    if (!result.okay())
        return result.err();

    return zl::res<list_t<T>, AllocationStatusCode>{
        std::in_place,
        M{
            .parent = parent_allocator,
            .memory = result.release(),
            .size = 0,
        },
    };
}

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

template <typename T>
auto list_t<T>::try_remove_at(size_t index) noexcept -> status_t
{
    ALLO_VALID_ARG_ASSERT(index < m.size);
    if (index >= m.size) {
        return StatusCode::IndexOutOfRange;
    }
    remove_at_unchecked(index);
    return StatusCode::Okay;
}

template <typename T> void list_t<T>::remove_at_unchecked(size_t index) noexcept
{
    ALLO_UNCHECKED_ASSERT(index < m.size);
    (m.memory.data()[index]).~T();
    for (size_t i = index; i < m.size - 1; ++i) {
        new (m.memory.data() + i) T(std::move(m.memory.data()[i + 1]));
    }
    --m.size;
}

template <typename T>
auto list_t<T>::try_realloc_if_needed() noexcept -> status_t
{
    // NOTE: reallocation here is very simple, because you can only insert
    // one item at a time.
    if (m.memory.size() <= m.size) {
        if (m.parent) {
            const auto newsize = static_cast<size_t>(std::ceil(
                static_cast<double>(m.memory.size()) * realloc_ratio));

            auto realloc_result =
                allo::realloc(m.parent.value(), m.memory, newsize);
            if (!realloc_result.okay())
                return StatusCode::AllocatorError;

            m.memory = realloc_result.release();
        } else {
            return StatusCode::OOM;
        }
    }
    return StatusCode::Okay;
}

template <typename T> zl::opt<T&> list_t<T>::try_get_at(size_t index) noexcept
{
    ALLO_VALID_ARG_ASSERT(index < m.size);
    if (index >= m.size)
        return {};
    return get_at_unchecked(index);
}

template <typename T>
zl::opt<const T&> list_t<T>::try_get_at(size_t index) const noexcept
{
    ALLO_VALID_ARG_ASSERT(index < m.size);
    if (index >= m.size)
        return {};
    return get_at_unchecked(index);
}

template <typename T> T& list_t<T>::get_at_unchecked(size_t index) noexcept
{
    ALLO_UNCHECKED_ASSERT(index < m.size);
    return m.memory.data()[index];
}

template <typename T>
const T& list_t<T>::get_at_unchecked(size_t index) const noexcept
{
    ALLO_UNCHECKED_ASSERT(index < m.size);
    return m.memory.data()[index];
}

} // namespace allo
