#pragma once

#include "allo/allocator_interfaces.h"
#include <type_traits>
namespace allo {
/// Create an allocator within another. Provide some initial allocation for the
/// child allocator to reside in.
/// Not thread safe: ie. this child allocator assumes that it has exclusive
/// access to its parent. If you want to make threadsafe child allocator(s),
/// look at the ThreadsafeAllocator's branch and branch_shared methods.
template <typename ChildAllocator, typename ParentAllocator>
inline ChildAllocator branch(ParentAllocator &parent,
                             zl::slice<uint8_t> initial_memory) noexcept
{
    constexpr bool is_valid_interface =
        std::is_base_of_v<ParentAllocator, detail::allocator_interface_t> &&
        detail::interface_has_alloc<ParentAllocator>() &&
        detail::interface_has_stack_free<ParentAllocator>() &&
        detail::interface_has_stack_realloc<ParentAllocator>();
    constexpr bool is_valid_allocator =
        std::is_base_of_v<ParentAllocator, detail::dynamic_allocator_base_t> &&
        detail::can_upcast_to<allocator_with<IAlloc, IStackFree, IStackRealloc>,
                              ParentAllocator>;
    static_assert(
        !(is_valid_interface && is_valid_allocator),
        "ParentAllocator type is both a valid interface and allocator. Is this "
        "a custom allocator with incorrect inheritance?");
    static_assert(is_valid_interface || is_valid_allocator,
                  "Invalid type provided for parent allocator");
    static_assert(
        std::is_base_of_v<ChildAllocator, detail::dynamic_allocator_base_t>,
        "Invalid type provided for child allocator");
}
} // namespace allo
