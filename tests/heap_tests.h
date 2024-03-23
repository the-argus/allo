#pragma once
#include "allo.h"

namespace allo::tests {
void allocate_480_bytes_related_objects(abstract_heap_allocator_t &heap);
void typed_alloc_realloc_free(abstract_heap_allocator_t &heap);
} // namespace allo::tests
