#include "allo/c_allocator.h"
#include "allo/structures/list.h"
#include "allo/structures/uninitialized_array.h"
// test header should be last
#include "test_header.h"

using namespace allo;
static_assert(!std::is_default_constructible_v<list_t<int>>,
              "list of ints is default constructible");

TEST_SUITE("list_t")
{
    TEST_CASE("Construction and type behavior")
    {
        SUBCASE("Making with c allocator")
        {
            c_allocator_t c;
            auto maybe_collection = list_t<int>::make_owned(c, 100);
            REQUIRE(maybe_collection.okay());
        }

        SUBCASE("Making with heap allocator")
        {
            c_allocator_t c;
            auto heap = allo::heap_allocator_t::make(
                            allo::alloc<uint8_t>(c, 4000).release())
                            .release();

            auto maybe_collection = list_t<uint8_t>::make_owned(heap, 2000);
            REQUIRE(maybe_collection.okay());
        }

        SUBCASE("make with static buffer")
        {
            allo::uninitialized_array_t<int, 120> mem;
            auto list = list_t<int>::make(mem);
        }
    }

    TEST_CASE("functionality")
    {
        SUBCASE("push back a bunch of ints and pop some off, also reallocate")
        {
            allo::c_allocator_t c;
            auto maybe_list = list_t<int>::make_owned(c, 2);
            REQUIRE(maybe_list.okay());
            list_t<int> list = maybe_list.release();

            std::array toadd = {1,  2,     3,    4,       345, 64556,
                                23, 23423, 8989, 9089234, 1234};

            for (int i : toadd) {
                auto put_res_1 = list.try_append(i);
                REQUIRE(put_res_1.okay());
                REQUIRE(list.try_get_at(list.items().size() - 1).has_value());
                REQUIRE(list.get_at_unchecked(list.items().size() - 1) == i);
                auto pus_res_2 = list.try_append(i);
                REQUIRE(pus_res_2.okay());
                REQUIRE(list.get_at_unchecked(list.items().size() - 1) == i);
                auto remove_res = list.try_remove_at(list.items().size() - 1);
                REQUIRE(remove_res.okay());
            }

            REQUIRE(zl::memcompare(list.items(), zl::slice<int>(toadd)));
        }

        SUBCASE("functionality with static buffer")
        {
            allo::uninitialized_array_t<int, 500> buf;
            auto list = list_t<int>::make(buf);

            std::array toadd = {1,  2,     3,    4,       345, 64556,
                                23, 23423, 8989, 9089234, 1234};

            for (int i : toadd) {
                auto put_res_1 = list.try_append(i);
                REQUIRE(put_res_1.okay());
                REQUIRE(list.try_get_at(list.items().size() - 1).has_value());
                REQUIRE(list.get_at_unchecked(list.items().size() - 1) == i);
                auto pus_res_2 = list.try_append(i);
                REQUIRE(pus_res_2.okay());
                REQUIRE(list.get_at_unchecked(list.items().size() - 1) == i);
                auto remove_res = list.try_remove_at(list.items().size() - 1);
                REQUIRE(remove_res.okay());
            }

            REQUIRE(zl::memcompare(list.items(), zl::slice<int>(toadd)));
        }

        SUBCASE("items stay in expected order after removal and insertion")
        {
            uninitialized_array_t<int, 500> buf;
            auto list = list_t<int>::make(buf);
            REQUIRE(list.items().size() == 0);
            for (size_t i = 0; i < 20; ++i) {
                auto res = list.try_append(i);
            }

            {
                std::array expected = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
                                       10, 11, 12, 13, 14, 15, 16, 17, 18, 19};
                REQUIRE(zl::memcompare(zl::slice<int>(expected), list.items()));
            }

            auto res = list.try_remove_at(3);
            REQUIRE(res.okay());

            {
                std::array expected = {0,  1,  2,  4,  5,  6,  7,  8,  9, 10,
                                       11, 12, 13, 14, 15, 16, 17, 18, 19};
                REQUIRE(zl::memcompare(zl::slice<int>(expected), list.items()));
            }

            res = list.try_remove_at(8);
            REQUIRE(res.okay());

            {
                std::array expected = {0,  1,  2,  4,  5,  6,  7,  8,  10,
                                       11, 12, 13, 14, 15, 16, 17, 18, 19};
                REQUIRE(zl::memcompare(zl::slice<int>(expected), list.items()));
            }

            res = list.try_insert_at(0, 20);
            REQUIRE(res.okay());
            {
                std::array expected = {20, 0,  1,  2,  4,  5,  6,  7,  8, 10,
                                       11, 12, 13, 14, 15, 16, 17, 18, 19};
                REQUIRE(zl::memcompare(zl::slice<int>(expected), list.items()));
            }
        }

        SUBCASE("items stay the same after reallocation")
        {
            allo::c_allocator_t c;
            auto list = list_t<int>::make_owned(c, 2).release();
            REQUIRE(list.capacity() == 2);

            auto res = list.try_append(0);
            REQUIRE(res.okay());
            REQUIRE(list.capacity() == 2);
            res = list.try_append(1);
            REQUIRE(res.okay());
            REQUIRE(list.capacity() == 2);

            {
                std::array intended_items = {0, 1};

                REQUIRE(zl::memcompare(list.items(),
                                       zl::slice<int>(intended_items)));
            }

            res = list.try_append(2);
            REQUIRE(res.okay());
            REQUIRE(list.capacity() > 2);
            res = list.try_append(3);

            {
                std::array intended_items = {0, 1, 2, 3};
                REQUIRE(zl::memcompare(list.items(),
                                       zl::slice<int>(intended_items)));
            }
        }
    }
}
