#include "allo.h"
#include "allo/structures/collection.h"
#include "test_header.h"

using namespace allo;
static_assert(!std::is_default_constructible_v<collection_t<int>>,
              "Collection of ints is default constructible");

TEST_SUITE("collection_t")
{
    TEST_CASE("Construction and type behavior")
    {
        SUBCASE("Making with c allocator")
        {
            c_allocator_t c;
            auto maybe_collection = collection_t<int>::make(c, 100);
            REQUIRE(maybe_collection.okay());
        }

        SUBCASE("Making with heap allocator")
        {
            c_allocator_t c;
            auto heap =
                heap_allocator_t::make(alloc<uint8_t>(c, 4000).release())
                    .release();

            auto maybe_collection = collection_t<uint8_t>::make(heap, 2000);
            REQUIRE(maybe_collection.okay());
        }
    }

    TEST_CASE("functionality")
    {
        SUBCASE("push back a bunch of ints and pop some off")
        {
            c_allocator_t c;
            auto maybe_collection = collection_t<int>::make(c, 100);
            REQUIRE(maybe_collection.okay());
            collection_t<int> collection = maybe_collection.release();

            std::array toadd = {1,  2,     3,    4,       345, 64556,
                                23, 23423, 8989, 9089234, 1234};

            for (int i : toadd) {
                auto a_res = collection.try_append(i);
                REQUIRE(a_res.okay());
                REQUIRE(*(collection.items().end().ptr() - 1) == i);
                auto e_res = collection.try_emplace(i);
                REQUIRE(e_res.okay());
            }
        }
    }
}
