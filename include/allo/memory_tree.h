#pragma once

#include "allo/allocator_interfaces.h"
#include <type_traits>
namespace allo {
template <typename ChildAllocator, typename ParentAllocator>
inline ChildAllocator make_single_branch(ParentAllocator &parent) noexcept
{
    static_assert(
        std::is_base_of_v<ParentAllocator, detail::dynamic_allocator_base_t> &&
            detail::can_upcast_to<
                allocator_with<IAlloc, IStackFree, IStackRealloc>,
                ParentAllocator>,
        "Invalid type provided for parent allocator");
    static_assert(
        std::is_base_of_v<ChildAllocator, detail::dynamic_allocator_base_t>,
        "Invalid type provided for child allocator");
}
} // namespace allo
