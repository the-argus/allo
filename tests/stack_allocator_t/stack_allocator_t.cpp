#include "allo/stack_allocator.hpp"
#include "test_header.hpp"
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
            stack_allocator_t ally(mem);
        }

        SUBCASE("move semantics")
        {
            std::array<uint8_t, 512> mem;
            stack_allocator_t ally(mem);

            mem[0] = 1;

            {
                stack_allocator_t ally_2(std::move(ally));
                ally_2.zero();
            }

            REQUIRE(mem[0] == 0);
        }

        SUBCASE("initialize with subslice of memory")
        {
            std::array<uint8_t, 512> mem{0};
            lib::slice_t<uint8_t> subslice(mem, 100, mem.size() - 200);
            for (auto byte : subslice) {
                // array should be 0 initialized
                REQUIRE(byte == 0);
            }

            // set all contents to 1
            std::fill(mem.begin(), mem.end(), 1);
            REQUIRE(mem[0] == 1);

            stack_allocator_t ally(subslice);
            // zero all memory inside of allocator
            ally.zero();

            // require that the correct elements of mem were zeroed
            size_t index = 0;
            for (auto byte : mem) {
                bool in_subslice = &mem[index] >= subslice.begin() &&
                                   &mem[index] < subslice.end();
                REQUIRE(byte == ((in_subslice) ? 0 : 1));
                ++index;
            }
        }
    }

    TEST_CASE("functionality")
    {
        SUBCASE("alloc array")
        {
            std::array<uint8_t, 512> mem;
            stack_allocator_t ally(mem);

            auto *my_ints = ally.alloc<std::array<int, 100>>();
            REQUIRE(my_ints);

            auto *original_int_location = my_ints;
            REQUIRE(ally.free(my_ints));

            auto *my_new_ints = ally.alloc<std::array<int, 100>>();
            REQUIRE(my_new_ints);
            REQUIRE(my_new_ints == original_int_location);
        }

        SUBCASE("OOM")
        {
            std::array<uint8_t, 512> mem;
            stack_allocator_t ally(mem);

            auto *arr = ally.alloc<std::array<uint8_t, 496>>();
            REQUIRE(arr);
            REQUIRE(ally.free(arr));
            REQUIRE(!ally.alloc<std::array<uint8_t, 512>>());
        }

        SUBCASE("Cant free a different type than the last one")
        {
            std::array<uint8_t, 512> mem;
            stack_allocator_t ally(mem);

            auto *guy = ally.alloc<int>();
            size_t fake;
            REQUIRE(!ally.free(&fake));
        }

        SUBCASE("allocating a bunch of different types and then freeing them "
                "in reverse order")
        {
            std::array<uint8_t, 512> mem;
            stack_allocator_t ally(mem);

            auto *set = ally.alloc<std::set<const char *>>();
            auto *vec = ally.alloc<std::vector<int>>();
            vec->push_back(10);
            vec->push_back(20);
            set->insert("hello");
            set->insert("nope");
            auto *opt = ally.alloc<std::optional<size_t>>();
            opt->emplace(10);

            // can't do it in the wrong order
            REQUIRE(!ally.free(vec));
            REQUIRE(!ally.free(set));

            REQUIRE(ally.free(opt));
            // still cant do it in the wrong order...
            REQUIRE(!ally.free(set));
            REQUIRE(ally.free(vec));
            REQUIRE(ally.free(set));
        }
    }
}
