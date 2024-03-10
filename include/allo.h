#pragma once

#include <allo/typed_freeing.h>
#include <allo/typed_allocation.h>
#include <allo/typed_reallocation.h>
#include <allo/detail/abstracts.h>

namespace allo {
using DynamicAllocatorRef = detail::allocator_common_t;
using DynamicStackAllocatorRef = detail::dynamic_stack_allocator_t;
using DynamicHeapAllocatorRef = detail::dynamic_heap_allocator_t;
}
