#pragma once
#include "allo.h"

namespace allo::tests {
void allocate_480_bytes_related_objects(HeapAllocatorDynRef heap);
void typed_alloc_realloc_free(HeapAllocatorDynRef heap);
}
