#pragma once

#include "allo/detail/abstracts.h"
#include <cstdint>
#include <ziglike/opt.h>

namespace allo {
class any_allocator_t
{
  public:
    enum class AllocatorReferenceType : uint8_t
    {
        Null,
        Basic,
        Stack,
        Heap,
    };

    inline any_allocator_t() noexcept
        : m(M{
              .ref = nullptr,
              .type = AllocatorReferenceType::Null,
          })
    {
    }

    inline constexpr any_allocator_t(
        detail::abstract_allocator_t &ally) noexcept
        : m(M{
              .ref = std::addressof(ally),
              .type = AllocatorReferenceType::Basic,
          })
    {
    }

    inline constexpr any_allocator_t(
        detail::abstract_heap_allocator_t &ally) noexcept
        : m(M{
              .ref = std::addressof(ally),
              .type = AllocatorReferenceType::Heap,
          })
    {
    }

    inline constexpr any_allocator_t(
        detail::abstract_stack_allocator_t &ally) noexcept
        : m(M{
              .ref = std::addressof(ally),
              .type = AllocatorReferenceType::Stack,
          })
    {
    }

    [[nodiscard]] inline constexpr AllocatorReferenceType type() const noexcept
    {
        return m.type;
    }

    [[nodiscard]] inline constexpr bool is_basic() const noexcept
    {
        return m.type == AllocatorReferenceType::Basic;
    }

    [[nodiscard]] inline constexpr bool is_stack() const noexcept
    {
        return m.type == AllocatorReferenceType::Stack;
    }

    [[nodiscard]] inline constexpr bool is_heap() const noexcept
    {
        return m.type == AllocatorReferenceType::Heap;
    }

    [[nodiscard]] inline constexpr bool is_null() const noexcept
    {
        return m.type == AllocatorReferenceType::Null;
    }

    [[nodiscard]] inline zl::opt<detail::abstract_heap_allocator_t &>
    get_heap() const noexcept
    {
        if (!is_heap())
            return {};
        return *static_cast<detail::abstract_heap_allocator_t *>(m.ref);
    }

    [[nodiscard]] inline constexpr detail::abstract_heap_allocator_t &
    get_heap_unchecked() const noexcept
    {
        assert(is_heap());
        return *static_cast<detail::abstract_heap_allocator_t *>(m.ref);
    }

    [[nodiscard]] inline zl::opt<detail::abstract_stack_allocator_t &>
    get_stack() const noexcept
    {
        if (!is_stack())
            return {};
        return *static_cast<detail::abstract_stack_allocator_t *>(m.ref);
    }

    [[nodiscard]] inline constexpr detail::abstract_stack_allocator_t &
    get_stack_unchecked() const noexcept
    {
        assert(is_stack());
        return *static_cast<detail::abstract_stack_allocator_t *>(m.ref);
    }

    [[nodiscard]] inline zl::opt<detail::abstract_allocator_t &>
    get_basic() const noexcept
    {
        if (!is_basic())
            return {};
        return *static_cast<detail::abstract_allocator_t *>(m.ref);
    }

    [[nodiscard]] inline constexpr detail::abstract_allocator_t &
    get_basic_unchecked() const noexcept
    {
        assert(is_basic());
        return *static_cast<detail::abstract_allocator_t *>(m.ref);
    }

    [[nodiscard]] inline constexpr detail::abstract_allocator_t &
    cast_to_basic() const noexcept
    {
        switch (m.type) {
        case AllocatorReferenceType::Basic:
            return get_basic_unchecked();
            break;
        case AllocatorReferenceType::Heap:
            return get_heap_unchecked();
            break;
        case AllocatorReferenceType::Stack:
            return get_stack_unchecked();
            break;
        case AllocatorReferenceType::Null:
            std::abort();
            break;
        }
    }

  private:
    struct M
    {
        void *ref;
        AllocatorReferenceType type;
    } m;
};
} // namespace allo
