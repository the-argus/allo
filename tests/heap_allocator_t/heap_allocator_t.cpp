#include "allo.h"
#include "allo/c_allocator.h"
#include "allo/heap_allocator.h"
#include "allo/make_into.h"
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
            heap_allocator_t heap = heap_allocator_t::make(mem);
            allo::free(global_allocator, mem);
        }
        SUBCASE("make owned")
        {
            c_allocator_t global_allocator;
            auto mem = alloc<uint8_t>(global_allocator, 2000).release();
            heap_allocator_t heap =
                heap_allocator_t::make_owning(mem, global_allocator);
        }
        SUBCASE("make_into")
        {
            c_allocator_t global_allocator;

            zl::slice<uint8_t> mem =
                allo::alloc<uint8_t>(global_allocator, 2000).release();

            {
                heap_allocator_t& heap =
                    allo::make_into<heap_allocator_t>(global_allocator, mem)
                        .release();

                auto mint = allo::alloc_one<int>(heap);
                REQUIRE(mint.okay());
            }

            {
                heap_allocator_t& heap =
                    allo::make_into<heap_allocator_t, MakeType::Owned>(
                        global_allocator, mem, global_allocator)
                        .release();
                auto mint = allo::alloc_one<int>(heap);
                REQUIRE(mint.okay());
            }
        }
    }

    TEST_CASE("functionality")
    {
        SUBCASE("generic allocator test - linked list")
        {
            c_allocator_t global_allocator;
            // NOTE: bytes required at time of writing by allocator type:
            // block allocator: 1825
            // heap allocator: 2776
            // stack allocator: 1875
            // heap allocator has a significant amount of bookkeeping space
            auto mem = alloc<uint8_t>(global_allocator, 2776).release();
            heap_allocator_t heap = heap_allocator_t::make(mem);
            tests::allocate_object_with_linked_list(heap);
            allo::free(global_allocator, mem);
        }

        SUBCASE("generic ref, large allocation")
        {
            tests::make_large_allocation_with_nonfailing_make<
                heap_allocator_t>();
        }

        SUBCASE("heap allocator_test - allocate and free one thing")
        {
            c_allocator_t global_allocator;
            auto mem = alloc<uint8_t>(global_allocator, 2000).release();
            heap_allocator_t heap =
                heap_allocator_t::make_owning(mem, global_allocator);
            tests::allocate_480_bytes_related_objects(heap);
            tests::typed_alloc_realloc_free(heap);
        }
    }
}
