#include "allo/block_allocator.h"
#include "allo/c_allocator.h"
#include "allo/typed_allocation.h"
#include "allo/typed_freeing.h"
#include "test_header.h"

using namespace allo;

TEST_SUITE("block_allocator_t")
{
    TEST_CASE("Construction and type behavior")
    {
        SUBCASE("Default construction")
        {
            c_allocator_t global_allocator;
            auto minitial_memory = allo::alloc<uint8_t>(global_allocator, 2000);
            REQUIRE(minitial_memory.okay());
            auto initial_memory = minitial_memory.release();
            auto mally = block_allocator_t::make(initial_memory,
                                                 global_allocator, 200, 2);
            REQUIRE(mally.okay());
            block_allocator_t ally = mally.release();

            auto mint = allo::alloc_one<int>(ally);
            REQUIRE(mint.okay());
            int &my_int = mint.release();
            auto status = allo::free_one(ally, my_int);
            REQUIRE(status.okay());
        }
    }
}
