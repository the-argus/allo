#include "allo/allocator_interfaces.h"
#include "allo/c_allocator.h"
#include "allo/stack_allocator.h"
#include "allo/typed_allocation.h"
#include "allo/typed_freeing.h"
#include "test_header.h"

using namespace allo;

TEST_SUITE("allocator interfaces")
{
    TEST_CASE("Upcasting")
    {
        SUBCASE("upcast to single interface")
        {
            auto makeint = [](detail::i_alloc &allocator)
                -> zl::res<int *, AllocationStatusCode> {
                auto mem_res =
                    allocator.alloc_bytes(sizeof(int), alignof(int), 0);
                if (!mem_res.okay())
                    return mem_res.err();

                return reinterpret_cast<int *>(mem_res.release().data());
            };

            std::array<uint8_t, 512> mem;
            stack_allocator_t stack(mem);

            auto maybe_int =
                makeint(upcast<stack_allocator_t, detail::i_alloc>(stack));
            REQUIRE(maybe_int.okay());

            c_allocator_t heap;
            auto maybe_int_2 =
                makeint(upcast<c_allocator_t, detail::i_alloc>(heap));
            REQUIRE(maybe_int_2.okay());
        }

        SUBCASE("upcast to single interface, use typed alloc")
        {
            auto makeint = [](detail::i_alloc &allocator)
                -> zl::res<int &, AllocationStatusCode> {
                return allo::alloc_one<int, detail::i_alloc>(allocator);
            };

            std::array<uint8_t, 512> mem;
            stack_allocator_t stack(mem);

            auto maybe_int =
                makeint(upcast<stack_allocator_t, detail::i_alloc>(stack));
            REQUIRE(maybe_int.okay());

            c_allocator_t heap;
            auto maybe_int_2 =
                makeint(upcast<c_allocator_t, detail::i_alloc>(heap));
            REQUIRE(maybe_int_2.okay());
        }
    }
}
