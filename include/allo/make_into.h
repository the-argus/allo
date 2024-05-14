#pragma once
#include "allo/ctti/typename.h"
#include "allo/detail/abstracts.h"
#include "allo/status.h"
#include <ziglike/res.h>

namespace allo {
enum class MakeType
{
    Owned,
    Unowned,
};

/// Construct an allocator in memory allocated by a parent allocator, and
/// register that allocator to be destructed by the parent.
/// It is undefined behavior to call the destructor of the returned allocator,
/// or to cause it to be deallocated before the parent allocator has a chance to
/// call its destruction callbacks.
///
/// If the parent allocator given can free, then on error all memory will be
/// cleaned up. However, if you pass in a scratch allocator, memory will not be
/// cleaned up on error.
///
/// Allocator: type of the allocator to make
/// ParentAllocator: the type of the allocator which is being used to create the
/// Allocator.
/// Args: additional arguments to pass to the make function, after the slice of
/// memory "mem"
///
/// allocator: the allocator which should be used to allocate a new Allocator
/// mem: the memory that the allocator will start with
/// args: additional arguments to pass to the make function
template <typename Allocator, MakeType maketype = MakeType::Unowned,
          typename ParentAllocator, typename... Args>
zl::res<Allocator&, AllocationStatusCode>
make_into(ParentAllocator& allocator, bytes_t mem, Args&&... args) noexcept
{
    static_assert(!std::is_reference_v<Allocator> &&
                      !std::is_pointer_v<Allocator>,
                  "Cannot construct a reference or pointer type using make.");
    static_assert(detail::is_real_allocator<Allocator>,
                  "The given allocator type cannot be constructed.");
    static_assert(detail::is_allocator<ParentAllocator>,
                  "The provided parent allocator is not a real allocator.");

    const size_t typehash =
#ifndef ALLO_DISABLE_TYPEINFO
#ifdef ALLO_USE_RTTI
        typeid(Allocator).hash_code();
#else
        ctti::nameof<Allocator>().hash();
#endif
#else
        0;
#endif

    // figure out at compile time whether we should make or make_owning
    auto makefunc =
        [mem](Args&&... args) -> zl::res<Allocator, AllocationStatusCode> {
        if constexpr (maketype == MakeType::Unowned) {
            return Allocator::make(mem, std::forward<Args>(args)...);
        } else {
            return Allocator::make_owning(mem, std::forward<Args>(args)...);
        }
    };

    auto construct_res = makefunc(std::forward<Args>(args)...);

    if (!construct_res.okay()) {
        return construct_res.err();
    }

    auto alloc_res = allocator.alloc_bytes(
        sizeof(Allocator),
        detail::nearest_alignment_exponent(alignof(Allocator)), typehash);
    if (!alloc_res.okay()) {
        return alloc_res.err();
    }

    bytes_t allocation = alloc_res.release();

    auto* ally = reinterpret_cast<Allocator*>(allocation.data());
    new (ally) Allocator(std::move(construct_res.release()));

    auto callback_status = ally->register_destruction_callback(
        [](void* data) { ((decltype(ally))data)->~Allocator(); }, ally);

    if (!callback_status.okay()) {
        // if this type can free, then free the allocator
        if constexpr (std::is_base_of_v<detail::abstract_stack_allocator_t,
                                        ParentAllocator>) {
            allocator.free_bytes(allocation, typehash);
        }
        return callback_status.err();
    }

    return *ally;
}
} // namespace allo
