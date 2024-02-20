#include "allo/oneshot_allocator.h"
#include "allo/stack_allocator.h"
#include "allo/typed_allocation.h"
#include "allo/typed_freeing.h"
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
            oneshot_allocator_t oneshot =
                oneshot_allocator_t::make(mem).release();
            stack_allocator_t ally =
                stack_allocator_t::make(
                    mem, upcast<allocator_with<IRealloc, IFree>>(oneshot))
                    .release();
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
            zl::slice<uint8_t> subslice(mem, 100, mem.size() - 200);
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
                bool in_subslice = &mem[index] >= subslice.begin().ptr() &&
                                   &mem[index] < subslice.end().ptr();
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

            auto maybe_my_ints = allo::alloc_one<std::array<int, 100>>(ally);
            REQUIRE(maybe_my_ints.okay());

            std::array<int, 100> &my_ints = maybe_my_ints.release();

            int *original_int_location = my_ints.data();
            auto free_status = allo::free_one(ally, my_ints);
            REQUIRE(free_status.okay());

            auto my_new_ints = allo::alloc_one<std::array<int, 100>>(ally);
            REQUIRE(my_new_ints.okay());
            REQUIRE((my_new_ints.release().data() == original_int_location));
        }

        SUBCASE("OOM")
        {
            std::array<uint8_t, 512> mem;
            stack_allocator_t ally(mem);

            auto arr_res = allo::alloc_one<std::array<uint8_t, 496>>(ally);
            REQUIRE(arr_res.okay());
            auto &arr = arr_res.release();
            REQUIRE(allo::free_one(ally, arr).okay());
            REQUIRE(!allo::alloc_one<std::array<uint8_t, 512>>(ally).okay());
        }

        SUBCASE("Cant free a different type than the last one")
        {
            std::array<uint8_t, 512> mem;
            stack_allocator_t ally(mem);

            auto guy_res = allo::alloc_one<int>(ally);
            size_t fake;
            REQUIRE(!allo::free_one(ally, fake).okay());
        }

        SUBCASE("allocating a bunch of different types and then freeing them "
                "in reverse order")
        {
            std::array<uint8_t, 512> mem;
            stack_allocator_t ally(mem);

            auto set_res = allo::construct_one<std::set<const char *>>(ally);
            REQUIRE(set_res.okay());
            auto &set = set_res.release();
            auto vec_res = allo::construct_one<std::vector<int>>(ally);
            REQUIRE(vec_res.okay());
            auto &vec = vec_res.release();
            vec.push_back(10);
            vec.push_back(20);
            set.insert("hello");
            set.insert("nope");
            auto opt_res = allo::construct_one<std::optional<size_t>>(ally);
            REQUIRE(opt_res.okay());
            auto &opt = opt_res.release();
            opt.emplace(10);

            // can't do it in the wrong order
            REQUIRE(!allo::destroy_one(ally, vec).okay());
            REQUIRE(!allo::destroy_one(ally, set).okay());

            REQUIRE(allo::destroy_one(ally, opt).okay());
            // still cant do it in the wrong order...
            REQUIRE(!allo::destroy_one(ally, set).okay());
            REQUIRE(allo::destroy_one(ally, vec).okay());
            REQUIRE(allo::destroy_one(ally, set).okay());
        }
    }
}
