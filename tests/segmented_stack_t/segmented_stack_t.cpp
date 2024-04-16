#include "allo/c_allocator.h"
#include "allo/scratch_allocator.h"
#include "allo/structures/segmented_stack.h"
// test header should be last
#include "test_header.h"

template <typename T> using stack = allo::segmented_stack_t<T>;
static_assert(!std::is_default_constructible_v<stack<int>>,
              "Segmented stack of ints is default constructible");

TEST_SUITE("segmented_stack_t")
{
    TEST_CASE("Construction and type behavior")
    {
        SUBCASE("Making with heap allocator")
        {
            allo::c_allocator_t c;
            auto heap = allo::heap_allocator_t::make(
                            allo::alloc<uint8_t>(c, 4000).release())
                            .release();

            auto maybe_stack = stack<uint8_t>::make_owned(heap, 1000);
            REQUIRE(maybe_stack.okay());
        }

        SUBCASE("make with scratch allocator")
        {
            allo::uninitialized_array_t<uint8_t, 240> mem;
            allo::scratch_allocator_t scratch =
                allo::scratch_allocator_t::make(mem);
            auto mystack = stack<int>::make(scratch, 50);
        }

        SUBCASE("move into function")
        {
            allo::c_allocator_t c;
            auto heap = allo::heap_allocator_t::make(
                            allo::alloc<uint8_t>(c, 4000).release())
                            .release();
            auto maybe_st = stack<int>::make_owned(heap, 100);
            REQUIRE(maybe_st.okay());
            stack<int> &st = maybe_st.release_ref();
            std::array toadd = {1,  2,     3,    4,       345, 64556,
                                23, 23423, 8989, 9089234, 1234};
            for (int item : toadd) {
                auto res = st.try_push(item);
                REQUIRE(res.okay());
            }

            // make sure the contents of the stack are as expected, by popping
            for (size_t i = 0; i < toadd.size(); ++i) {
                int end = st.end_unchecked();
                REQUIRE(end == toadd[toadd.size() - i - 1]);
                st.pop();
            }

            // push back everything we just popped to check
            for (int item : toadd) {
                auto res = st.try_push(item);
                REQUIRE(res.okay());
            }

            // make sure that the same thing works after move constructor
            auto consumer = [&toadd](stack<int> &&s) {
                stack<int> ourstack(std::move(s));
                for (size_t i = 0; i < toadd.size(); ++i) {
                    int end = ourstack.end_unchecked();
                    REQUIRE(end == toadd[toadd.size() - i - 1]);
                    ourstack.pop();
                }
            };

            consumer(std::move(st));
        }
    }

    TEST_CASE("functionality")
    {
        SUBCASE("push back a bunch of ints and pop some off, also reallocate")
        {
            allo::c_allocator_t c;
            auto heap = allo::heap_allocator_t::make(
                            allo::alloc<uint8_t>(c, 4000).release())
                            .release();
            auto maybe_stack = stack<int>::make_owned(heap, 2);
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

            for (size_t i = 0; i < toadd.size(); ++i) {
                int end = stack.end_unchecked();
                REQUIRE(end == toadd[toadd.size() - i - 1]);
                stack.pop();
            }
        }

        SUBCASE("functionality with static buffer and scratch allocator")
        {
            allo::uninitialized_array_t<uint8_t, 500> mem;
            allo::scratch_allocator_t scratch =
                allo::scratch_allocator_t::make(mem);
            auto maybe_st = stack<int>::make(scratch, 50);
            REQUIRE(maybe_st.okay());
            stack<int> &st = maybe_st.release_ref();

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

            {
                REQUIRE(toadd.size() == st.size());
                size_t index = 0;
                st.for_each([&toadd, &index](int &item) {
                    REQUIRE(toadd[index] == item);
                    ++index;
                });
            }

            for (size_t i = 0; i < toadd.size(); ++i) {
                int end = st.end_unchecked();
                REQUIRE(end == toadd[toadd.size() - i - 1]);
                st.pop();
            }
        }

        SUBCASE("items stay the same after reallocation")
        {
            allo::c_allocator_t c;
            auto heap = allo::heap_allocator_t::make(
                            allo::alloc<uint8_t>(c, 4000).release())
                            .release();
            auto st = stack<int>::make_owned(heap, 1).release();

            auto res = st.try_push(0);
            REQUIRE(res.okay());
            res = st.try_push(1);
            REQUIRE(res.okay());

            {
                std::array intended_items = {0, 1};

                for (size_t i = 0; i < intended_items.size(); ++i) {
                    int end = st.end_unchecked();
                    REQUIRE(end ==
                            intended_items[intended_items.size() - i - 1]);
                    st.pop();
                }
            }

            // push back the items we removed
            res = st.try_push(0);
            REQUIRE(res.okay());
            res = st.try_push(1);
            REQUIRE(res.okay());

            for (size_t i = 2; i < 200; ++i) {
                res = st.try_push(i);
                REQUIRE(res.okay());
            }

            for (int64_t i = 199; i > 0; --i) {
                int end = st.end_unchecked();
                REQUIRE(end == i);
                st.pop();
            }
        }
    }
}
