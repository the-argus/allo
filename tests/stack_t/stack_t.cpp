#include "allo.h"
#include "allo/structures/stack.h"
#include "test_header.h"

template <typename T>
using stack = allo::stack_t<T>;
static_assert(!std::is_default_constructible_v<stack<int>>,
              "Collection of ints is default constructible");

TEST_SUITE("stack_t")
{
    TEST_CASE("Construction and type behavior")
    {
        SUBCASE("Making with c allocator")
        {
            allo::c_allocator_t c;
            auto maybe_collection = stack<int>::make(c, 100);
            REQUIRE(maybe_collection.okay());
        }

        SUBCASE("Making with heap allocator")
        {
            allo::c_allocator_t c;
            auto heap =
                allo::heap_allocator_t::make(alloc<uint8_t>(c, 4000).release())
                    .release();

            auto maybe_collection = stack<uint8_t>::make(heap, 2000);
            REQUIRE(maybe_collection.okay());
        }
    }

    TEST_CASE("functionality")
    {
        SUBCASE("push back a bunch of ints and pop some off")
        {
            allo::c_allocator_t c;
            auto maybe_stack = stack<int>::make(c, 100);
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
            }
        }
    }
}
