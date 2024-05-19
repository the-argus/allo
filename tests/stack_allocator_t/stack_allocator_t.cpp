#define ALLO_ALLOW_DESTRUCTORS // we allocate a std::set and std::vector
                               // TODO: make this not necessary
#include "allo/make_into.h"
#include "allo/stack_allocator.h"
#include "allo/typed_allocation.h"
#include "allo/typed_freeing.h"
#include "generic_allocator_tests.h"
#include "test_header.h"
#include <array>
#include <optional>
#include <set>
#include <vector>

using namespace allo;

TEST_SUITE("stack_allocator_t")
{
    TEST_CASE("Construction and type behavior")
    {
        SUBCASE("Default construction")
        {
            std::array<uint8_t, 512> mem;
            auto ally = stack_allocator_t::make(mem);
        }

        SUBCASE("move semantics")
        {
            std::array<uint8_t, 512> mem;

            auto ally = stack_allocator_t::make(mem);

            auto maybe_myint = allo::alloc_one<int>(ally);
            REQUIRE(maybe_myint.okay());
            int& myint = maybe_myint.release();
            void* address_of_int = &myint;
            myint = 10;

            {
                stack_allocator_t ally_2(std::move(ally));
                auto status = allo::free_one(ally_2, myint);
                // myint is invalid right here
                REQUIRE(status.okay());
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

            auto ally = stack_allocator_t::make(subslice);

            uint8_t& a_byte = allo::alloc_one<uint8_t>(ally).release();
            REQUIRE(a_byte == 1); // alloc one does not zero-initialize memory,
                                  // and the whole buffer was ones
        }

        SUBCASE("make_into")
        {
            c_allocator_t global_allocator;

            zl::slice<uint8_t> mem =
                allo::alloc<uint8_t>(global_allocator, 2000).release();

            {
                stack_allocator_t& stack =
                    allo::make_into<stack_allocator_t>(global_allocator, mem)
                        .release();
                auto mint = allo::alloc_one<int>(stack);
                REQUIRE(mint.okay());
            }

            {
                stack_allocator_t& stack =
                    allo::make_into<stack_allocator_t, MakeType::Owned>(
                        global_allocator, mem, global_allocator)
                        .release();
                auto mint = allo::alloc_one<int>(stack);
                REQUIRE(mint.okay());
            }
        }
    }

    TEST_CASE("functionality")
    {
        SUBCASE("alloc array")
        {
            std::array<uint8_t, 512> mem;
            auto ally = stack_allocator_t::make(mem);

            auto maybe_my_ints = allo::alloc_one<std::array<int, 88>>(ally);
            REQUIRE(maybe_my_ints.okay());

            std::array<int, 88>& my_ints = maybe_my_ints.release();

            int* original_int_location = my_ints.data();
            auto free_status = allo::free_one(ally, my_ints);
            REQUIRE(free_status.okay());

            auto my_new_ints = allo::alloc_one<std::array<int, 88>>(ally);
            REQUIRE(my_new_ints.okay());
            REQUIRE((my_new_ints.release().data() == original_int_location));
        }

        SUBCASE("generic ref, large allocation")
        {
            tests::make_large_allocation_with_nonfailing_make<
                stack_allocator_t>();
        }

        SUBCASE("allocate many things then free then reallocate")
        {
            c_allocator_t c;
            auto buffer = alloc<uint8_t>(c, 50000).release();

            stack_allocator_t stack = stack_allocator_t::make(buffer);

            // nest another stack within the current one, inside this test
            tests::allocate_free_then_allocate_again<stack_allocator_t>(stack);

            allo::free(c, buffer);
        }

        SUBCASE("generic ref, allocate linked list")
        {
            // TODO: this test is only supposed to need something like 830
            // bytes? but because of the bookkeeping data in each stack
            // allocation, it uses an extra kilobyte...
            // look at the block allocator's version, which uses a similar
            // amount. maybe this is actually roughly the amount to expect from
            // the algorithm? but it seems like in theory the algorithm should
            // only alloc about 830.
            std::array<uint8_t, 1875> mem; // just barely enough memory for the
                                           // linked list of strings
            auto ally = stack_allocator_t::make(mem);
            tests::allocate_object_with_linked_list(ally);
        }

        SUBCASE("OOM")
        {
            std::array<uint8_t, 512> mem;
            auto ally = stack_allocator_t::make(mem);

            auto arr_res = allo::alloc_one<std::array<uint8_t, 494>>(ally);
            REQUIRE(arr_res.okay());
            auto& arr = arr_res.release();
            REQUIRE(allo::free_one(ally, arr).okay());
            REQUIRE(!allo::alloc_one<std::array<uint8_t, 512>>(ally).okay());
        }

        SUBCASE("Cant free a different type than the last one")
        {
            std::array<uint8_t, 512> mem;
            auto ally = stack_allocator_t::make(mem);

            auto guy_res = allo::alloc_one<int>(ally);
            size_t fake;
#ifdef NDEBUG
            // NOTE: only check this in release mode, in debug mode this asserts
            REQUIRE(!allo::free_one(ally, fake).okay());
#endif
        }

        SUBCASE("allocating a bunch of different types and then freeing them "
                "in reverse order")
        {
            std::array<uint8_t, 512> mem;
            auto ally = stack_allocator_t::make(mem);

            auto set_res = allo::construct_one<std::set<const char*>>(ally);
            REQUIRE(set_res.okay());
            auto& set = set_res.release();
            auto vec_res = allo::construct_one<std::vector<int>>(ally);
            REQUIRE(vec_res.okay());
            auto& vec = vec_res.release();
            vec.push_back(10);
            vec.push_back(20);
            set.insert("hello");
            set.insert("nope");
            auto opt_res = allo::construct_one<std::optional<size_t>>(ally);
            REQUIRE(opt_res.okay());
            auto& opt = opt_res.release();
            opt.emplace(10);

#ifdef NDEBUG
            // can't do it in the wrong order
            REQUIRE(!allo::destroy_one(ally, vec).okay());
            REQUIRE(!allo::destroy_one(ally, set).okay());
#endif

            REQUIRE(allo::destroy_one(ally, opt).okay());
#ifdef NDEBUG
            // still cant do it in the wrong order...
            REQUIRE(!allo::destroy_one(ally, set).okay());
#endif
            REQUIRE(allo::destroy_one(ally, vec).okay());
            REQUIRE(allo::destroy_one(ally, set).okay());
        }

        SUBCASE("register destruction callback")
        {
            std::array<uint8_t, 512> mem;
            int test = 0;
            {
                auto stack = stack_allocator_t::make(mem);
                auto status = stack.register_destruction_callback(
                    [](void* test_int) { *((int*)test_int) = 1; }, &test);
                REQUIRE(status.okay());
                REQUIRE(test == 0);
            }
            REQUIRE(test == 1);
        }
    }
}
