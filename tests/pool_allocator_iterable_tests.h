#pragma once
#include "pool_allocator_base_tests.hpp"

template <typename options_t,
          template <typename T, options_t options, typename index_t = size_t>
          class Pool>
class pool_allocator_iterable_tests
    : public pool_allocator_base_tests<options_t, Pool>
{
    using parent = pool_allocator_base_tests<options_t, Pool>;

  public:
    static void no_iterations_for_empty()
    {
        using pool = Pool<int, parent::c_options>;

        pool mypool(100);

        size_t loops = 0;
        for (auto item : mypool) {
            ++loops;
        }
        REQUIRE(loops == 0);
    }

    static void pool_that_has_been_added_to()
    {
        using pool = Pool<int, parent::c_options>;

        pool mypool(100);

        typename pool::handle_t myint_1 = mypool.alloc_new(2).release();
        typename pool::handle_t myint_2 = mypool.alloc_new(0).release();
        typename pool::handle_t myint_3 = mypool.alloc_new(1).release();
        typename pool::handle_t myint_4 = mypool.alloc_new(4).release();

        size_t loops = 0;
        for (auto item : mypool) {
            ++loops;
        }
        REQUIRE(loops == 4);
    }

    static void pool_that_has_been_added_to_and_removed_from()
    {
        using pool = Pool<int, parent::c_options>;

        pool mypool(100);

        typename pool::handle_t myint_1 = mypool.alloc_new(2).release();
        typename pool::handle_t myint_2 = mypool.alloc_new(0).release();
        typename pool::handle_t myint_3 = mypool.alloc_new(1).release();
        typename pool::handle_t myint_4 = mypool.alloc_new(4).release();

        REQUIRE(mypool.free(myint_2).okay());
        REQUIRE(mypool.free(myint_3).okay());

        size_t loops = 0;
        for (auto item : mypool) {
            REQUIRE((item == 2 || item == 4));
            ++loops;
        }
        REQUIRE(loops == 2);
    }
};
