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
#include <cmath>
#include <ziglike/slice.h>

namespace allo {
template <typename T> class stack_t
{
  public:
    using type = T;

    static constexpr float realloc_ratio = 1.5f;

    /// Create a new stack-t by creating a buffer in a given heap allocator
    /// initial_items is the number of items to reserve space for initially.
    /// If zero, will be rounded up to one.
    [[nodiscard]] static zl::res<stack_t, AllocationStatusCode>
    make(detail::abstract_heap_allocator_t &parent_allocator,
         size_t initial_items) noexcept;

    stack_t() = delete;
    stack_t(const stack_t &) = delete;
    stack_t &operator=(const stack_t &) = delete;
    stack_t(stack_t &&) noexcept = default;
    stack_t &operator=(stack_t &&) noexcept = default;

    [[nodiscard]] constexpr zl::slice<const T> items() const noexcept;

    [[nodiscard]] constexpr zl::slice<T> items() noexcept;

    [[nodiscard]] constexpr zl::opt<T &> end() noexcept;

    constexpr void pop() noexcept;

    [[nodiscard]] constexpr size_t capacity() const noexcept;

    template <typename... Args>
    [[nodiscard]] inline allocation_status_t try_push(Args &&...args) noexcept
    {
        if (m.capacity <= m.items.size()) [[unlikely]] {
            auto status = try_realloc();
            if (!status.okay()) [[unlikely]] {
                return status.err();
            }
        }
        assert(m.capacity > m.items.size());
        push_unchecked(std::forward<Args>(args)...);
        return AllocationStatusCode::Okay;
    }

    [[nodiscard]] constexpr T &end_unchecked() noexcept;

    inline ~stack_t() noexcept
    {
        allo::free(m.parent, zl::raw_slice(*m.items.begin().ptr(), m.capacity));
    }

  private:
    struct M
    {
        zl::slice<T> items;
        size_t capacity;
        detail::abstract_heap_allocator_t &parent;
    } m;

    constexpr size_t calculate_new_size() noexcept;

    constexpr allocation_status_t try_realloc() noexcept;

    template <typename... Args>
    inline constexpr void push_unchecked(Args &&...args) noexcept
    {
        // grow items by one
        m.items = zl::raw_slice<T>(*m.items.begin().ptr(), m.items.size() + 1);
        new (std::addressof(m.items.data()[m.items.size() - 1]))
            T(std::forward<Args>(args)...);
    }

  public:
    inline constexpr stack_t(M members) noexcept : m(members) {}
};

template <typename T>
inline constexpr zl::slice<const T> stack_t<T>::items() const noexcept
{
    return m.items;
}

template <typename T> inline constexpr zl::slice<T> stack_t<T>::items() noexcept
{
    return m.items;
}

template <typename T>
inline zl::res<stack_t<T>, AllocationStatusCode>
stack_t<T>::make(detail::abstract_heap_allocator_t &parent_allocator,
                 size_t initial_items) noexcept
{
    const size_t actual_initial = initial_items == 0 ? 1 : initial_items;

    auto maybe_initial = allo::alloc<T>(parent_allocator, actual_initial);
    if (!maybe_initial.okay())
        return maybe_initial.err();

    auto &initial = maybe_initial.release_ref();

    return zl::res<stack_t, AllocationStatusCode>{
        std::in_place, M{
                           .items = zl::slice<T>(initial, 0, 0),
                           .capacity = initial.size(),
                           .parent = parent_allocator,
                       }};
}

template <typename T> constexpr size_t stack_t<T>::calculate_new_size() noexcept
{
    const auto res = static_cast<size_t>(
        std::ceil(static_cast<float>(m.items.size()) * realloc_ratio));
    assert(res - m.items.size() >= 1);
    return res;
}

template <typename T>
constexpr allocation_status_t stack_t<T>::try_realloc() noexcept
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

template <typename T> constexpr zl::opt<T &> stack_t<T>::end() noexcept
{
    if (m.items.size() == 0)
        return {};
    return end_unchecked();
}

template <typename T> constexpr T &stack_t<T>::end_unchecked() noexcept
{
    assert(m.items.size() > 0);
    return *(m.items.end().ptr() - 1);
}

template <typename T> constexpr void stack_t<T>::pop() noexcept
{
    if (m.items.size() == 0)
        return;
    remove_unchecked(m.items.size() - 1);
}

} // namespace allo
