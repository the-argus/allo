#pragma once

#include <allo/detail/abstracts.h>
#include <allo/typed_allocation.h>
#include <allo/typed_freeing.h>
#include <allo/typed_reallocation.h>

namespace allo {
using abstract_allocator_t = detail::abstract_allocator_t;
using abstract_stack_allocator_t = detail::abstract_stack_allocator_t;
using abstract_heap_allocator_t = detail::abstract_heap_allocator_t;
using abstract_threadsafe_heap_allocator_t =
    detail::abstract_threadsafe_heap_allocator_t;
} // namespace allo
