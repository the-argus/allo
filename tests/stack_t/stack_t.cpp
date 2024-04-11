#include "allo/c_allocator.h"
#include "allo/structures/stack.h"
#include "allo/structures/uninitialized_array.h"
// test header should be last
#include "test_header.h"

template <typename T> using stack = allo::stack_t<T>;
static_assert(!std::is_default_constructible_v<stack<int>>,
              "Collection of ints is default constructible");

TEST_SUITE("stack_t")
{
    TEST_CASE("Construction and type behavior")
    {
        SUBCASE("Making with c allocator")
        {
            allo::c_allocator_t c;
            auto maybe_collection = stack<int>::make_owned(c, 100);
            REQUIRE(maybe_collection.okay());
        }

        SUBCASE("Making with heap allocator")
        {
            allo::c_allocator_t c;
            auto heap = allo::heap_allocator_t::make(
                            allo::alloc<uint8_t>(c, 4000).release())
                            .release();

            auto maybe_collection = stack<uint8_t>::make_owned(heap, 2000);
            REQUIRE(maybe_collection.okay());
        }

        SUBCASE("make with static buffer")
        {
            allo::uninitialized_array_t<int, 120> mem;
            auto mystack = stack<int>::make(mem);
        }
    }

    TEST_CASE("functionality")
    {
        SUBCASE("push back a bunch of ints and pop some off, also reallocate")
        {
            allo::c_allocator_t c;
            auto maybe_stack = stack<int>::make_owned(c, 2);
            REQUIRE(maybe_stack.okay());
            stack<int> stack = maybe_stack.release();

            std::array toadd = {1,  2,     3,    4,       345, 64556,
                                23, 23423, 8989, 9089234, 1234};

            for (int i : toadd) {
                auto put_res_1 = stack.try_push(i);
                REQUIRE(put_res_1.okay());
                REQUIRE(stack.end().has_value());
                REQUIRE(stack.end().value() == i);
                auto pus_res_2 = stack.try_push(i);
                REQUIRE(pus_res_2.okay());
                REQUIRE(stack.end().value() == i);
                stack.pop();
            }

            REQUIRE(zl::memcompare(stack.items(), zl::slice<int>(toadd)));
        }

        SUBCASE("functionality with static buffer")
        {
            allo::uninitialized_array_t<int, 500> buf;
            auto st = stack<int>::make(buf);

            std::array toadd = {1,  2,     3,    4,       345, 64556,
                                23, 23423, 8989, 9089234, 1234};

            for (int i : toadd) {
                auto put_res_1 = st.try_push(i);
                REQUIRE(put_res_1.okay());
                REQUIRE(st.end().has_value());
                REQUIRE(st.end().value() == i);
                auto pus_res_2 = st.try_push(i);
                REQUIRE(pus_res_2.okay());
                REQUIRE(st.end().value() == i);
                st.pop();
            }

            REQUIRE(zl::memcompare(st.items(), zl::slice<int>(toadd)));
        }

        SUBCASE("items stay the same after reallocation")
        {
            allo::c_allocator_t c;
            auto st = stack<int>::make_owned(c, 2).release();
            REQUIRE(st.capacity() == 2);

            auto res = st.try_push(0);
            REQUIRE(res.okay());
            REQUIRE(st.capacity() == 2);
            res = st.try_push(1);
            REQUIRE(res.okay());
            REQUIRE(st.capacity() == 2);

            {
                std::array intended_items = {0, 1};

                REQUIRE(
                    zl::memcompare(st.items(), zl::slice<int>(intended_items)));
            }

            res = st.try_push(2);
            REQUIRE(res.okay());
            REQUIRE(st.capacity() > 2);
            res = st.try_push(3);

            {
                std::array intended_items = {0, 1, 2, 3};
                REQUIRE(
                    zl::memcompare(st.items(), zl::slice<int>(intended_items)));
            }
        }
    }
}
