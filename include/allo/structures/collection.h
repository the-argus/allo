#pragma once
#include "allo.h"
#include <ziglike/slice.h>

namespace allo {
template <typename T> class collection_t
{
  public:
    using type = T;

    static constexpr float realloc_ratio = 1.5f;

    enum class StatusCode : uint8_t
    {
        Okay,
        ResultReleased,
        OutOfRange,
        NotFound,
        InvalidArgument,
        AllocatorFailure,
    };

    using status_t = zl::status<StatusCode>;

    /// Create a new collection_t by creating a buffer in a given heap allocator
    /// initial_items is the number of items to reserve space for initially.
    /// Must be greater than zero.
    static zl::res<collection_t, StatusCode>
    make(abstract_heap_allocator_t &parent_allocator,
         size_t initial_items) noexcept;

    collection_t() = delete;
    collection_t(const collection_t &) = delete;
    collection_t &operator=(const collection_t &) = delete;
    collection_t(collection_t &&) noexcept = default;
    collection_t &operator=(collection_t &&) noexcept = default;

    [[nodiscard]] constexpr zl::slice<const T> items() const noexcept;

    [[nodiscard]] constexpr zl::slice<T> items() noexcept;

    [[nodiscard]] constexpr size_t capacity() const noexcept;

    [[nodiscard]] inline allocation_status_t try_append(const T &item) noexcept
    {
        if (m.capacity <= m.items.size()) [[unlikely]] {
            auto status = try_realloc();
            if (!status.okay()) [[unlikely]] {
                return status.err();
            }
        }
        assert(m.capacity > m.items.size());
        append_unchecked(item);
        return AllocationStatusCode::Okay;
    }

    template <typename... Args>
    [[nodiscard]] inline allocation_status_t
    try_emplace(Args &&...args) noexcept
    {
        if (m.capacity <= m.items.size()) [[unlikely]] {
            auto status = try_realloc();
            if (!status.okay()) [[unlikely]] {
                return status.err();
            }
        }
        assert(m.capacity > m.items.size());
        emplace_unchecked(std::forward<Args>(args)...);
        return AllocationStatusCode::Okay;
    }

    [[nodiscard]] constexpr status_t try_remove(size_t index) noexcept;

    // unsafe api
    constexpr void remove_unchecked(size_t index) noexcept;

    inline constexpr void append_unchecked(const T &item) noexcept
    {
        // grow items by one
        m.items = zl::raw_slice<T>(*m.items.begin().ptr(), m.items.size() + 1);
        new (std::addressof(m.items.data()[m.items.size() - 1])) T(item);
    }

    template <typename... Args>
    inline constexpr void emplace_unchecked(Args &&...args) noexcept
    {
        // grow items by one
        m.items = zl::raw_slice<T>(*m.items.begin().ptr(), m.items.size() + 1);
        new (std::addressof(m.items.data()[m.items.size() - 1]))
            T(std::forward<Args>(args)...);
    }

    inline ~collection_t()
    {
        allo::free(m.parent, zl::raw_slice(*m.items.begin().ptr(), m.capacity));
    }

  private:
    struct M
    {
        zl::slice<T> items;
        size_t capacity;
        abstract_heap_allocator_t &parent;
    } m;

    constexpr size_t calculate_new_size() noexcept;

    constexpr allocation_status_t try_realloc() noexcept;

  public:
    inline constexpr collection_t(M members) noexcept : m(members) {}
};

template <typename T>
inline constexpr zl::slice<const T> collection_t<T>::items() const noexcept
{
    return m.items;
}

template <typename T>
inline constexpr zl::slice<T> collection_t<T>::items() noexcept
{
    return m.items;
}

template <typename T>
constexpr auto collection_t<T>::try_remove(size_t index) noexcept -> status_t
{
    static_assert(
        std::is_destructible_v<T>,
        "Cannot remove from a collection which holds an un-destructible type.");
    static_assert(std::is_nothrow_destructible_v<T>,
                  "Cannot call remove on a collection_t whose contents may "
                  "throw when destroyed.");
    static_assert(std::is_nothrow_move_constructible_v<T>,
                  "Cannot remove item from collection which holds a type which "
                  "may throw when move constructed.");

    if (index >= m.items.size()) [[unlikely]]
        return StatusCode::OutOfRange;

    remove_unchecked(index);
    return StatusCode::Okay;
}

template <typename T>
constexpr void collection_t<T>::remove_unchecked(size_t index) noexcept
{
    T &target = m.items.data()[index];
    target.~T();
    const size_t newsize = m.items.size() - 1;
    if (index != newsize) {
        new (std::addressof(target)) T(std::move(m.items.data()[newsize]));
    }
    m.items = zl::slice<T>(m.items, 0, newsize);
}

template <typename T>
inline auto collection_t<T>::make(abstract_heap_allocator_t &parent_allocator,
                                  size_t initial_items) noexcept
    -> zl::res<collection_t, StatusCode>
{
    if (initial_items == 0) [[unlikely]] {
        assert(initial_items != 0);
        return StatusCode::InvalidArgument;
    }

    auto maybe_initial = allo::alloc<T>(parent_allocator, initial_items);
    if (!maybe_initial.okay())
        return StatusCode::AllocatorFailure;

    auto &initial = maybe_initial.release_ref();

    return zl::res<collection_t, StatusCode>{
        std::in_place, M{
                           .items = zl::slice<T>(initial, 0, 0),
                           .capacity = initial.size(),
                           .parent = parent_allocator,
                       }};
}

template <typename T>
constexpr size_t collection_t<T>::calculate_new_size() noexcept
{
    const auto res = static_cast<size_t>(
        std::ceil(static_cast<float>(m.items.size()) * realloc_ratio));
    assert(res - m.items.size() >= 1);
    return res;
}

template <typename T>
constexpr allocation_status_t collection_t<T>::try_realloc() noexcept
{
    static_assert(std::is_trivially_copyable_v<T>,
                  "collection assumes trivially copyable types for T... until "
                  "allo implements nontrivial realloc this is the only thing "
                  "that makes sense");
    auto result = allo::realloc(m.parent, m.items, calculate_new_size());
    if (!result.okay())
        return result.err();
    auto &new_mem = result.release_ref();
    m.capacity = new_mem.size();
    m.items = zl::slice<T>(new_mem, 0, m.items.size());
    return AllocationStatusCode::Okay;
}

} // namespace allo
