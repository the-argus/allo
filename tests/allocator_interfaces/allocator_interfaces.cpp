#include "allo/allocator_interfaces.h"
#include "allo/c_allocator.h"
#include "allo/stack_allocator.h"
#include "allo/typed_allocation.h"
#include "allo/typed_freeing.h"
#include "test_header.h"

#include "allo/ctti/typename.h"

using namespace allo;

// clang-format off
static_assert(std::is_same_v<allocator_with<IStackFree>, detail::i_stack_free>);
static_assert(std::is_same_v<allocator_with<IStackRealloc>, detail::i_stack_realloc>);
static_assert(std::is_same_v<allocator_with<IStackRealloc, IStackFree>, detail::i_stack_realloc_i_stack_free>);
static_assert(std::is_same_v<allocator_with<IFree>, detail::i_free>);
static_assert(std::is_same_v<allocator_with<IStackRealloc, IFree>, detail::i_stack_realloc_i_free>);
static_assert(std::is_same_v<allocator_with<IRealloc>, detail::i_realloc>);
static_assert(std::is_same_v<allocator_with<IRealloc, IStackFree>, detail::i_realloc_i_stack_free>);
static_assert(std::is_same_v<allocator_with<IRealloc, IFree>, detail::i_realloc_i_free>);

static_assert(std::is_same_v<allocator_with<IAlloc>, detail::i_alloc>);
static_assert(std::is_same_v<allocator_with<IAlloc, IStackFree>, detail::i_alloc_i_stack_free>);
static_assert(std::is_same_v<allocator_with<IAlloc, IStackRealloc>, detail::i_alloc_i_stack_realloc>);
static_assert(std::is_same_v<allocator_with<IAlloc, IStackRealloc, IStackFree>, detail::i_alloc_i_stack_realloc_i_stack_free>);
static_assert(std::is_same_v<allocator_with<IAlloc, IFree>, detail::i_alloc_i_free>);
static_assert(std::is_same_v<allocator_with<IAlloc, IStackRealloc, IFree>, detail::i_alloc_i_stack_realloc_i_free>);
static_assert(std::is_same_v<allocator_with<IAlloc, IRealloc>, detail::i_alloc_i_realloc>);
static_assert(std::is_same_v<allocator_with<IAlloc, IRealloc, IStackFree>, detail::i_alloc_i_realloc_i_stack_free>);
static_assert(std::is_same_v<allocator_with<IAlloc, IRealloc, IFree>, detail::i_alloc_i_realloc_i_free>);
// clang-format on

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
