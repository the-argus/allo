#pragma once

#include "allo/allocator_interfaces.h"
#include <type_traits>
namespace allo {
namespace detail {
template <typename ThreadsafeAllocator>
inline constexpr bool is_valid_threadsafe_allocator()
{
    bool free_good =
        (detail::has_stack_free(ThreadsafeAllocator::enum_value) &&
         detail::has_free(ThreadsafeAllocator::enum_value)) ||
        (!detail::has_stack_free(ThreadsafeAllocator::enum_value) &&
         !detail::has_free(ThreadsafeAllocator::enum_value));
    bool realloc_good =
        (detail::has_stack_realloc(ThreadsafeAllocator::enum_value) &&
         detail::has_realloc(ThreadsafeAllocator::enum_value)) ||
        (!detail::has_stack_realloc(ThreadsafeAllocator::enum_value) &&
         !detail::has_realloc(ThreadsafeAllocator::enum_value));
    return free_good && realloc_good;
}
} // namespace detail

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

    if constexpr (std::is_base_of_v<detail::threadsafe_dynamic_allocator_base_t,
                                    ChildAllocator>) {
        static_assert(
            detail::is_valid_threadsafe_allocator<ChildAllocator>(),
            "Cannot create a threadsafe allocator which is only capable of "
            "stackfree and not free, or stackrealloc and not realloc.");
        constexpr auto child_bits = detail::get_bits_for_index<uint8_t(ChildAllocator::enum_value)>();
        if constexpr (is_valid_interface) {
            constexpr auto parent_bits = ParentAllocator::interfaces;
            static_assert((child_bits & parent_bits) == parent_bits);
        } else {
            constexpr auto parent_bits = detail::get_bits_for_index<uint8_t(ParentAllocator::enum_value)>();
            static_assert((child_bits & parent_bits) == parent_bits);
        }
    }
}
} // namespace allo
