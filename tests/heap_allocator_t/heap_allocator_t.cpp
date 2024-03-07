#include "allo/c_allocator.h"
#include "allo/heap_allocator.h"
#include "allo/typed_allocation.h"
#include "allo/typed_freeing.h"
#include "generic_allocator_tests.h"
#include "heap_tests.h"
#include "test_header.h"

#include <ziglike/stdmem.h>

using namespace allo;

TEST_SUITE("heap_allocator_t")
{
    TEST_CASE("Construction and type behavior")
    {
        SUBCASE("make unowned")
        {
            c_allocator_t global_allocator;
            auto mem = alloc<uint8_t>(global_allocator, 2000).release();
            heap_allocator_t heap = heap_allocator_t::make(mem).release();
            allo::free(global_allocator, mem);
        }
        SUBCASE("make owned")
        {
            c_allocator_t global_allocator;
            auto mem = alloc<uint8_t>(global_allocator, 2000).release();
            heap_allocator_t heap =
                heap_allocator_t::make_owned(mem, global_allocator).release();
        }
    }

    TEST_CASE("functionality")
    {
        SUBCASE("generic allocator test - linked list")
        {
            c_allocator_t global_allocator;
            // NOTE: bytes required at time of writing by allocator type:
            // block allocator: 1825
            // heap allocator: 2315
            // stack allocator: 1875
            // heap allocator has a significant amount of bookkeeping space
            auto mem = alloc<uint8_t>(global_allocator, 2315).release();
            heap_allocator_t heap =
                heap_allocator_t::make_owned(mem, global_allocator).release();
            tests::allocate_object_with_linked_list(heap);
        }

        SUBCASE("heap allocator_test - allocate and free one thing")
        {
            c_allocator_t global_allocator;
            auto mem = alloc<uint8_t>(global_allocator, 2000).release();
            heap_allocator_t heap =
                heap_allocator_t::make_owned(mem, global_allocator).release();
            tests::allocate_480_bytes_related_objects(heap);
        }
    }
}
