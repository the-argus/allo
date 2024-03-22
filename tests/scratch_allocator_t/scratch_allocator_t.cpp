#define ALLO_ALLOW_DESTRUCTORS
#define ALLO_ALLOW_NONTRIVIAL_COPY
#include "allo/make_into.h"
#include "allo/scratch_allocator.h"
#include "allo/typed_allocation.h"
#include "generic_allocator_tests.h"
#include "test_header.h"
#include <array>
#include <optional>

using namespace allo;

TEST_SUITE("scratch_allocator_t")
{
    TEST_CASE("Construction and type behavior")
    {
        SUBCASE("Default construction")
        {
            std::array<uint8_t, 512> mem;

            auto maybe_ally = scratch_allocator_t::make(mem);
            REQUIRE(maybe_ally.okay());
            scratch_allocator_t ally = maybe_ally.release();
        }

        SUBCASE("move semantics")
        {
            std::array<uint8_t, 512> mem;

            auto maybe_ally = scratch_allocator_t::make(mem);
            REQUIRE(maybe_ally.okay());
            scratch_allocator_t ally = maybe_ally.release();

            auto maybe_myint = allo::alloc_one<int>(ally);
            REQUIRE(maybe_myint.okay());
            int &myint = maybe_myint.release();
            void *address_of_int = &myint;
            myint = 10;

            {
                scratch_allocator_t ally_2(std::move(ally));
                myint = allo::alloc_one<int>(ally_2).release();
                // myint is valid again
                REQUIRE(&myint == address_of_int);
            }
        }

        SUBCASE("initialize with subslice of memory")
        {
            std::array<uint8_t, 512> mem{0};
            zl::slice<uint8_t> subslice(mem, 100, mem.size() - 200);
            for (auto byte : subslice) {
                // array should be 0 initialized
                REQUIRE(byte == 0);
            }

            // set all contents to 1
            std::fill(mem.begin(), mem.end(), 1);
            REQUIRE(mem[0] == 1);

            auto ally = scratch_allocator_t::make(subslice).release();

            uint8_t &a_byte = allo::alloc_one<uint8_t>(ally).release();
            REQUIRE(a_byte == 1); // alloc one does not zero-initialize memory,
                                  // and the whole buffer was ones
        }

        SUBCASE("make_into")
        {
            c_allocator_t global_allocator;

            zl::slice<uint8_t> mem =
                allo::alloc<uint8_t>(global_allocator, 2000).release();

            {
                scratch_allocator_t &scratch =
                    allo::make_into<scratch_allocator_t>(global_allocator, mem)
                        .release();

                auto mint = allo::alloc_one<int>(scratch);
                REQUIRE(mint.okay());
            }

            {
                scratch_allocator_t &scratch =
                    allo::make_into<scratch_allocator_t, MakeType::Owned>(
                        global_allocator, mem, global_allocator)
                        .release();
                auto mint = allo::alloc_one<int>(scratch);
                REQUIRE(mint.okay());
            }
        }
    }

    TEST_CASE("functionality")
    {
        SUBCASE("alloc array")
        {
            std::array<uint8_t, 512> mem;
            auto ally = scratch_allocator_t::make(mem).release();

            auto maybe_my_ints = allo::alloc_one<std::array<int, 88>>(ally);
            REQUIRE(maybe_my_ints.okay());

            std::array<int, 88> &my_ints = maybe_my_ints.release();
        }

        SUBCASE("generic ref, allocate linked list")
        {
            // TODO: this test is only supposed to need something like 830
            // bytes? where is the memory going? or is my math just wrong
            std::array<uint8_t, 945> mem; // just barely enough memory for the
                                          // linked list of strings
            auto ally = scratch_allocator_t::make(mem).release();
            tests::allocate_object_with_linked_list(ally);
        }

        SUBCASE("OOM")
        {
            std::array<uint8_t, 512> mem;
            auto ally = scratch_allocator_t::make(mem).release();

            auto arr_res = allo::alloc_one<std::array<uint8_t, 494>>(ally);
            REQUIRE(arr_res.okay());
            auto &arr = arr_res.release();
            REQUIRE(!allo::alloc_one<std::array<uint8_t, 512>>(ally).okay());
        }

        SUBCASE("register destruction callback")
        {
            std::array<uint8_t, 512> mem;
            int test = 0;
            {
                auto stack = scratch_allocator_t::make(mem).release();
                auto status = stack.register_destruction_callback(
                    [](void *test_int) { *((int *)test_int) = 1; }, &test);
                REQUIRE(status.okay());
                REQUIRE(test == 0);
            }
            REQUIRE(test == 1);
        }

        SUBCASE("callbacks passed as rvalue or as lvalue")
        {
            std::array<uint8_t, 100> real_mem;

            static size_t destructions = 0;
            static size_t constructions = 0;
            struct DestroyCounter
            {
                std::unique_ptr<int> cleanup;
                DestroyCounter() : cleanup(std::make_unique<int>(10))
                {
                    ++constructions;
                }
                ~DestroyCounter() { ++destructions; }
            };

            {
                scratch_allocator_t scratch =
                    scratch_allocator_t::make(real_mem).release();

                auto &counter_1 =
                    allo::construct_one<DestroyCounter>(scratch).release();
                auto &counter_2 =
                    allo::construct_one<DestroyCounter>(scratch).release();
                auto &counter_3 =
                    allo::construct_one<DestroyCounter>(scratch).release();

                auto status = scratch.register_destruction_callback(
                    [](void *d) -> void {
                        ((DestroyCounter *)d)->~DestroyCounter();
                    },
                    &counter_1);
                REQUIRE(status.okay());

                static size_t meaningless = 0;
                auto test = [](void *d) -> void {
                    ((DestroyCounter *)d)->~DestroyCounter();
                    ++meaningless;
                };
                status =
                    scratch.register_destruction_callback(test, &counter_2);
                REQUIRE(status.okay());
                status =
                    scratch.register_destruction_callback(test, &counter_3);
                REQUIRE(status.okay());
            }
            REQUIRE(destructions == 3);
            REQUIRE(constructions == 3);
        }
    }
}
