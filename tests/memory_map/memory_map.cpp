#include "allo/memory_map_alloc.h"
#include "test_header.h"

constexpr size_t bytes_in_gb = 1000000000;

TEST_SUITE("memory mapping alloc")
{
    TEST_CASE("basic functionality")
    {
        SUBCASE(
            "allocating a small amount and reallocating within page boundary")
        {
            void *initial_allocation = mm_alloc(1);
            REQUIRE(initial_allocation);
            *(int *)initial_allocation = 42;
            // NOTE: 32 is a implementation detail. that's now much space is
            // left on allocations for tracking size.
            int realloc_result =
                mm_realloc(initial_allocation, mm_get_page_size() - 32);
            REQUIRE(*(int *)initial_allocation == 42);
            REQUIRE(realloc_result == 0);
            int free_result = mm_free(initial_allocation);
            REQUIRE(free_result == 0);
        }
        SUBCASE("reallocating which requires a new page")
        {
            // this test will fail on systems without a gig of free ram
            void *initial_allocation = mm_alloc(1);
            REQUIRE(initial_allocation);
            *(int *)initial_allocation = 42;
            int realloc_result =
                mm_realloc(initial_allocation, mm_get_page_size() + 1);
            REQUIRE(realloc_result == 0);
            REQUIRE(*(int *)initial_allocation == 42);
            int free_result = mm_free(initial_allocation);
            REQUIRE(free_result == 0);
        }
    }
    TEST_CASE("errors returned")
    {
        SUBCASE("overallocating")
        {
            // this test will create a false negative on systems
            // with 100 GB of free ram. eat the rich
            void *initial_allocation = mm_alloc(bytes_in_gb * 100UL);
            REQUIRE(initial_allocation == nullptr);
        }
    }
}
