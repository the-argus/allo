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

        SUBCASE("Making with reservation allocator")
        {
            auto maybe_reserve = reservation_allocator_t::make(
                reservation_allocator_t::options_t{
                    .committed = 1,
                    .additional_pages_reserved = 10,
                });
            REQUIRE(maybe_reserve.okay());
            auto &reserve = maybe_reserve.release_ref();

            auto maybe_collection = collection_t<uint8_t>::make(reserve, 4000);
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
                REQUIRE(*collection.items().end().ptr() == i);
                REQUIRE(a_res.okay());
                auto e_res = collection.try_emplace(i);
                REQUIRE(e_res.okay());
            }
        }
    }
}
