#pragma once
#include "allo/detail/abstracts.h"

namespace allo::detail {
inline constexpr bool is_threadsafe_runtime(abstract_allocator_t &allocator)
{
    switch (allocator.type()) {
    case AllocatorType::CAllocator:
        return true;
    default:
        return false;
    }
}
} // namespace allo::detail
