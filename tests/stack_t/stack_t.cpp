#include "allo.h"
#include "allo/structures/stack.h"
#include "test_header.h"

using namespace allo;
static_assert(!std::is_default_constructible_v<stack_t<int>>,
              "Collection of ints is default constructible");

TEST_SUITE("stack_t")
{
    TEST_CASE("Construction and type behavior")
    {
        SUBCASE("Making with c allocator")
        {
            c_allocator_t c;
            auto maybe_collection = stack_t<int>::make(c, 100);
            REQUIRE(maybe_collection.okay());
        }

        SUBCASE("Making with heap allocator")
        {
            c_allocator_t c;
            auto heap =
                heap_allocator_t::make(alloc<uint8_t>(c, 4000).release())
                    .release();

            auto maybe_collection = stack_t<uint8_t>::make(heap, 2000);
            REQUIRE(maybe_collection.okay());
        }
    }

    TEST_CASE("functionality")
    {
        SUBCASE("push back a bunch of ints and pop some off")
        {
            c_allocator_t c;
            auto maybe_collection = stack_t<int>::make(c, 100);
            REQUIRE(maybe_collection.okay());
            stack_t<int> collection = maybe_collection.release();

            std::array toadd = {1,  2,     3,    4,       345, 64556,
                                23, 23423, 8989, 9089234, 1234};

            for (int i : toadd) {
                auto put_res_1 = collection.try_put(i);
                REQUIRE(put_res_1.okay());
                REQUIRE(*(collection.items().end().ptr() - 1) == i);
                auto pus_res_2 = collection.try_put(i);
                REQUIRE(pus_res_2.okay());
            }
        }
    }
}
