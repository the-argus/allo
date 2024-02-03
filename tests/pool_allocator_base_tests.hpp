#pragma once

#include "allo/allocator_interfaces.hpp"
#include "allo/c_allocator.hpp"
#include "doctest.h"

template <typename options_t,
          template <typename T, options_t options, typename index_t = size_t>
          class Pool>
class pool_allocator_base_tests
{
  public:
    static constexpr options_t c_options{
        .allocator = allo::c_allocator,
        .allocation_type = allo::interfaces::AllocationType::Component,
        .reallocating = true,
    };

    static void construction()
    {
        using pool = Pool<int, c_options>;
        // you have to specify the initial reservation
        pool mypool(0);
    }

    static void destruction()
    {
        static size_t destructions = 0;
        struct increment_on_destroy
        {
            int whatever;
            ~increment_on_destroy() { ++destructions; }
        };

        using pool = Pool<increment_on_destroy, c_options>;
        {
            REQUIRE(destructions == 0);
            pool test(100);
            REQUIRE(destructions == 0);
        }
        REQUIRE(destructions == 0);
        {
            REQUIRE(destructions == 0);
            pool test(100);
            REQUIRE(destructions == 0);
            for (size_t i = 0; i < 50; ++i) {
                auto handle = test.alloc_new();
            }
            REQUIRE(destructions == 0);
        }
        REQUIRE(destructions == 50);
    }

    static void construction_with_reservation()
    {
        using pool = Pool<int, c_options>;

        pool mypool(1000);
    }

    static void use_in_other_functions()
    {
        using pool = Pool<int, c_options>;

        auto makefiveints = [](pool &thepool)
            -> lib::result_t<std::array<typename pool::handle_t, 5>,
                             typename pool::alloc_err_code_e> {
            std::array results{
                thepool.alloc_new(1), thepool.alloc_new(2),
                thepool.alloc_new(3), thepool.alloc_new(4),
                thepool.alloc_new(5),
            };

            for (const auto &res : results) {
                if (!res.okay()) {
                    return res.status();
                }
            }

            /// Return by reference here to avoid copying and then destroying
            /// all these results. not thread safe ofc
            return std::array{
                results[0].release(), results[1].release(),
                results[2].release(), results[3].release(),
                results[4].release(),
            };
        };

        for (size_t i = 0; i < 10; ++i) {
            pool mypool(i);
            {
                auto allocated_ints_res = makefiveints(mypool);
                auto allocated_ints = allocated_ints_res.release_ref();

                size_t index = 0;
                for (auto inthandle : allocated_ints) {
                    ++index;
                    REQUIRE((mypool.get(inthandle).release() == index));
                }

                for (auto inthandle : allocated_ints) {
                    REQUIRE((mypool.free(inthandle).okay()));
                }
            }
            // same thing but remove the elements backwards and use release()
            // instead of release_ref()
            {
                auto allocated_ints_res = makefiveints(mypool);
                auto allocated_ints = allocated_ints_res.release();

                size_t index = 0;
                for (auto inthandle : allocated_ints) {
                    ++index;
                    REQUIRE((mypool.get(inthandle).release() == index));
                }

                for (auto iter = allocated_ints.rbegin();
                     iter != allocated_ints.rend(); ++iter) {
                    REQUIRE((mypool.free(*iter).okay()));
                }
            }
        }
    }

    static void get_functionality()
    {
        using pool = Pool<int, c_options>;

        pool mypool(100);

        typename pool::handle_t myint_1 = mypool.alloc_new(2).release();
        typename pool::handle_t myint_2 = mypool.alloc_new(0).release();
        typename pool::handle_t myint_3 = mypool.alloc_new(1).release();
        typename pool::handle_t myint_4 = mypool.alloc_new(4).release();

        REQUIRE(mypool.size() == 4);
        REQUIRE(mypool.capacity() == 100);
        REQUIRE(mypool.spots_available() == 96);

        REQUIRE(mypool.get(myint_1).release() == 2);
        REQUIRE(mypool.get(myint_2).release() == 0);
        REQUIRE(mypool.get(myint_3).release() == 1);
        REQUIRE(mypool.get(myint_4).release() == 4);

        mypool.free(myint_2);
        mypool.free(myint_3);
        REQUIRE(mypool.size() == 2);
        REQUIRE(mypool.capacity() == 100);
        REQUIRE(mypool.spots_available() == 98);
        // NOTE: it is now UB to try and get() myint_2 and myint_3
        REQUIRE(mypool.get(myint_1).release() == 2);
        REQUIRE(mypool.get(myint_4).release() == 4);
    }

    static void no_copy()
    {
        static int copies = 0;
        struct increment_on_copy
        {
            int one = 1;
            float two = 2;
            // NOLINTNEXTLINE
            inline constexpr increment_on_copy(int one, float two)
                : one(one), two(two)
            {
            }
            inline increment_on_copy &operator=(const increment_on_copy &other)
            {
                if (&other == this)
                    return *this;
                ++copies;
                one = other.one;
                two = other.two;
                return *this;
            }
            inline increment_on_copy(const increment_on_copy &other)
            {
                *this = other;
            }
        };

        using pool = Pool<increment_on_copy, c_options>;

        REQUIRE(copies == 0);
        pool mypool(100);
        REQUIRE(copies == 0);
        auto item_1 = mypool.alloc_new(1, 824).release();
        auto item_2 = mypool.alloc_new(2345, 3).release();
        auto item_3 = mypool.alloc_new(8972, 93).release();
        auto item_4 = mypool.alloc_new(2487, 908).release();
        REQUIRE(copies == 0);
        REQUIRE(mypool.get(item_1).release().one == 1);
        REQUIRE(mypool.get(item_1).release().two == 824);
        REQUIRE(copies == 0);
        auto copied = mypool.get(item_2).release();
        REQUIRE(copies == 1);
        auto &notcopied = mypool.get(item_2).release();
        REQUIRE(copies == 1);
        REQUIRE(mypool.free(item_1).okay());
        REQUIRE(mypool.free(item_2).okay());
        REQUIRE(mypool.free(item_3).okay());
        REQUIRE(mypool.free(item_4).okay());
        REQUIRE(copies == 1);
    }
};
