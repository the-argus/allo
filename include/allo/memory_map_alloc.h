#pragma once
#include "memory_map.h"

struct mm_allocation_size_block_t
{
    size_t size;
};

// returns a 32-byte aligned allocation which can be reallocated with mm_realloc
// without needing to change any pointers
void* mm_alloc(size_t bytes)
{
    mm_memory_map_result_t res = mm_memory_map(nullptr, bytes);
    if (res.code != 0) {
        return nullptr;
    }

    // ensure that the returned value is aligned enough to write a size block to it
    static_assert(alignof(mm_allocation_size_block_t) == 8, "memory_map_alloc is written for systems where size_t is 8 byte aligned.");
    static_assert(sizeof(mm_allocation_size_block_t) <= 32, "Allocation size block does not fit before allocations on this system");
    if ((uint64_t)res.data & 0b11111) {
        // mm_memory_map did not return the expected 32-byte aligned address
        return nullptr;
    }
    *(mm_allocation_size_block_t*)res.data = (mm_allocation_size_block_t){ .size = bytes };

    return ((uint8_t*)res.data) + 32;
}

void mm_realloc(void* block, size_t newsize)
{

}

void mm_free(void* block)
{}