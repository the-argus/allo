#pragma once
#include "memory_map.h"

#ifdef __cplusplus
extern "C"
{
#endif

    struct mm_allocation_size_block_t
    {
        size_t size;
    };

    // Returns a 32-byte aligned allocation which can be reallocated with
    // mm_realloc without needing to change any pointers.
    // Note that 32 bytes before this pointer is a size block which will be
    // used by mm_realloc and mm_free. If it is corrupted, you may be unable
    // to free the pages allocated with mm_alloc.
    inline void *mm_alloc(size_t bytes)
    {
        // the user thinks this allocation is 32 bytes smaller than it is
        mm_memory_map_result_t res = mm_memory_map(nullptr, bytes + 32);
        if (res.code != 0) {
            return nullptr;
        }

        // ensure that the returned value is aligned enough to write a size
        // block to it
        static_assert(alignof(mm_allocation_size_block_t) >= 8,
                      "memory_map_alloc is written for systems where size_t is "
                      "8 byte aligned or greater.");
        static_assert(sizeof(mm_allocation_size_block_t) <= 32,
                      "Allocation size block does not fit before allocations "
                      "on this system");
        if (((uint64_t)res.data & 0b11111) != 0) {
            // mm_memory_map did not return the expected 32-byte aligned address
            return nullptr;
        }
        *(mm_allocation_size_block_t *)res.data =
            (mm_allocation_size_block_t){.size = bytes};

        return ((uint8_t *)res.data) + 32;
    }

    // if all went okay, returns 0
    // on linux:
    //   - 12 == OOM (ENOMEM)
    inline int mm_realloc(void *address, size_t new_size)
    {
        if (((uint64_t)address & 0b11111) != 0) {
            return -1;
        }

        mm_allocation_size_block_t *size_block =
            (mm_allocation_size_block_t *)(((uint8_t *)address) - 32);

        mm_memory_map_resize_options_t options =
            (mm_memory_map_resize_options_t){
                .address = size_block,
                .old_size = size_block->size,
                // the user thinks this allocation is 32 bytes smaller than it
                // is
                .new_size = new_size + 32,
            };

        return mm_memory_map_resize(&options);
    }

    inline int mm_free(void *address)
    {
        if (((uint64_t)address & 0b11111) != 0) {
            return -1;
        }

        mm_allocation_size_block_t *size_block =
            (mm_allocation_size_block_t *)(((uint8_t *)address) - 32);
        return mm_memory_unmap(size_block, size_block->size);
    }

#ifdef __cplusplus
}
#endif
