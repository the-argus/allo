#pragma once
#include "allo/abstracts.h"

#ifndef ALLO_DISABLE_TYPEINFO
#ifndef ALLO_USE_RTTI
#include "ctti/typename.h"
#endif
#endif

namespace allo {
template <typename T, typename Allocator>
inline zl::res<zl::slice<T>, AllocationStatusCode>
realloc(Allocator &allocator, zl::slice<T> original, size_t new_size) noexcept
{
    static_assert(
        detail::can_upcast<Allocator,
                           detail::dynamic_stack_allocator_t>::type::value,
        "Cannot use given type to perform reallocations");
    static_assert(std::is_move_constructible_v<T> ||
                      std::is_copy_constructible_v<T>,
                  "Type cannot be moved or copied and cannot be reallocated. "
                  "Try realloc_stable instead.");
    return AllocationStatusCode::InvalidArgument;
}

template <typename T, typename Allocator>
inline zl::res<zl::slice<T>, AllocationStatusCode>
realloc_stable(Allocator &allocator, zl::slice<T> original,
               size_t new_size) noexcept
{
    static_assert(
        detail::can_upcast<Allocator,
                           detail::dynamic_stack_allocator_t>::type::value,
        "Cannot use given type to perform reallocations");
    return AllocationStatusCode::InvalidArgument;
}
} // namespace allo
