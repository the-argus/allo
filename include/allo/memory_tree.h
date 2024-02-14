#pragma once

#include "allo/allocator_interfaces.h"
#include <type_traits>
namespace allo {
template <typename ChildAllocator, typename ParentAllocator>
inline ChildAllocator make_single_branch(ParentAllocator &parent) noexcept
{
    static_assert(
        std::is_base_of_v<ParentAllocator, detail::dynamic_allocator_base_t>,
        "Invalid type provided for parent allocator");
    static_assert(
        std::is_base_of_v<ChildAllocator, detail::dynamic_allocator_base_t>,
        "Invalid type provided for child allocator");
    static_assert(detail::get_bits_for_index<uint8_t(Allocator::enum_value)>())
        detail::interface_bits[]
}
} // namespace allo
