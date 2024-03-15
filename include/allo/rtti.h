#pragma once
#include "allo.h"

namespace allo {
inline const char *allocator_typename(AllocatorDynRef allocator) noexcept
{
    using Type = detail::AllocatorType;
    auto type = *reinterpret_cast<Type *>(allocator.ref);
    switch (type) {
    case Type::CAllocator:
        return "c_allocator_t";
    case Type::BlockAllocator:
        return "block_allocator_t";
    case Type::StackAllocator:
        return "stack_allocator_t";
    case Type::ScratchAllocator:
        return "scratch_allocator_t";
    case Type::OneshotAllocator:
        return "oneshot_allocator_t";
    case Type::ReservationAllocator:
        return "reservation_allocator_t";
    case Type::HeapAllocator:
        return "heap_allocator_t";
    default:
        return "<unknown allocator>";
    }
}
} // namespace allo
