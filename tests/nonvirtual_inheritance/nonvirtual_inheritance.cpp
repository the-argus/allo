#include "allo.h"
#include "allo/c_allocator.h"
#include "allo/stack_allocator.h"
#include "allo/typed_allocation.h"
#include "test_header.h"
#include <array>

#include "allo/ctti/typename.h"

using namespace allo;

static_assert(detail::alignment_exponent(alignof(int)) == 2);
static_assert(detail::alignment_exponent(alignof(size_t)) == 3);
static_assert(detail::alignment_exponent(16) == 4);
static_assert(detail::alignment_exponent(32) == 5);
static_assert(detail::alignment_exponent(64) == 6);

TEST_SUITE("allocator interfaces")
{
    TEST_CASE("Upcasting")
    {
        SUBCASE("upcast to single interface")
        {
            auto makeint = [](abstract_allocator_t& allocator)
                -> zl::res<int*, AllocationStatusCode> {
                auto mem_res = allo::alloc<uint8_t>(allocator, sizeof(int) * 2);
                if (!mem_res.okay())
                    return mem_res.err();

                return reinterpret_cast<int*>(mem_res.release().data());
            };

            std::array<uint8_t, 512> mem;
            stack_allocator_t stack = stack_allocator_t::make(mem);

            auto maybe_int = makeint(stack);
            REQUIRE(maybe_int.okay());

            c_allocator_t heap;
            auto maybe_int_2 = makeint(heap);
            REQUIRE(maybe_int_2.okay());
        }

        SUBCASE("upcast interface")
        {
            std::array<uint8_t, 512> mem;
            auto stack = stack_allocator_t::make(mem);
            abstract_allocator_t& stackalloc = stack;
        }

        SUBCASE("upcast to single interface, use typed alloc")
        {
            auto makeint = [](abstract_allocator_t& allocator)
                -> zl::res<int&, AllocationStatusCode> {
                return allo::alloc_one<int>(allocator);
            };

            std::array<uint8_t, 512> mem;
            stack_allocator_t stack = stack_allocator_t::make(mem);

            auto maybe_int = makeint(stack);
            REQUIRE(maybe_int.okay());

            c_allocator_t heap;
            auto maybe_int_2 = makeint(heap);
            REQUIRE(maybe_int_2.okay());
        }
    }
}
