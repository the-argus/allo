#include "allo.h"
#include "allo/block_allocator.h"
#include "allo/c_allocator.h"
#include "allo/oneshot_allocator.h"
#include "allo/typed_freeing.h"
#include "generic_allocator_tests.h"
#include "heap_tests.h"
#include "test_header.h"

#include <ziglike/stdmem.h>

using namespace allo;
static_assert(detail::nearest_alignment_exponent(1) == 0);
static_assert(detail::nearest_alignment_exponent(3) == 0);
static_assert(detail::nearest_alignment_exponent(67) == 0);
static_assert(detail::nearest_alignment_exponent(2) == 1);
static_assert(detail::nearest_alignment_exponent(4) == 2);
static_assert(detail::nearest_alignment_exponent(8) == 3);
static_assert(detail::nearest_alignment_exponent(16) == 4);
static_assert(detail::nearest_alignment_exponent(32) == 5);
static_assert(detail::nearest_alignment_exponent(64) == 6);

TEST_SUITE("block_allocator_t")
{
    TEST_CASE("Construction and type behavior")
    {
        SUBCASE("Default construction")
        {
            c_allocator_t global_allocator;
            auto mally = block_allocator_t::make(
                allo::alloc<uint8_t>(global_allocator, 2000).release(),
                global_allocator, 200);
            REQUIRE(mally.okay());
            block_allocator_t ally = mally.release();

            auto mint = allo::alloc_one<int>(ally);
            REQUIRE(mint.okay());
            int &my_int = mint.release();
            auto status = allo::free_one(ally, my_int);
            REQUIRE(status.okay());
        }
    }
    TEST_CASE("functionality")
    {
        SUBCASE("related objects, similar structure to linked list")
        {
            c_allocator_t global_allocator;

            {
                // 256 bytes per block, 32 byte aligned blocks
                auto ally =
                    block_allocator_t::make(
                        // this memory will be cleaned up by the allocator
                        allo::alloc<uint8_t>(global_allocator, 2000).release(),
                        global_allocator, 256)
                        .release();
                tests::allocate_480_bytes_related_objects(ally);
                tests::typed_alloc_realloc_free(ally);
            }

            // this memory will leak since we aren't using a real allocator
            tests::allocate_480_bytes_related_objects(global_allocator);
            tests::typed_alloc_realloc_free(global_allocator);
        }

        SUBCASE("OOM when using oneshot allocator layer")
        {
            c_allocator_t global_allocator;

            {
                auto oneshot =
                    oneshot_allocator_t::make_owned(
                        allo::alloc<uint8_t>(global_allocator, 32UL * 4)
                            .release(),
                        global_allocator)
                        .release();
                // 256 bytes per block, 32 byte aligned blocks
                auto ally =
                    block_allocator_t::make(oneshot.shoot(), oneshot, 32)
                        .release();
                auto one = alloc_one<int>(ally);
                REQUIRE(one.okay());
                auto two = alloc_one<int>(ally);
                REQUIRE(two.okay());
                auto three = alloc_one<int>(ally);
                REQUIRE(three.okay());
                auto four = alloc_one<int>(ally);
                REQUIRE(four.okay());
                auto five = alloc_one<int>(ally);
                REQUIRE(five.err() == AllocationStatusCode::OOM);
            }
        }

        SUBCASE("No OOM when using global allocator directly")
        {
            c_allocator_t global_allocator;

            {
                // 256 bytes per block, 32 byte aligned blocks
                auto ally = block_allocator_t::make(
                                allo::alloc<uint8_t>(global_allocator, 32UL * 4)
                                    .release(),
                                global_allocator, 32)
                                .release();
                auto one = alloc_one<int>(ally);
                REQUIRE(one.okay());
                auto two = alloc_one<int>(ally);
                REQUIRE(two.okay());
                auto three = alloc_one<int>(ally);
                REQUIRE(three.okay());
                auto four = alloc_one<int>(ally);
                REQUIRE(four.okay());
                auto five = alloc_one<int>(ally);
                REQUIRE(five.okay());
            }
        }

        SUBCASE("generic tests")
        {
            c_allocator_t global_allocator;

            // 256 bytes per block, 32 byte aligned blocks
            auto ally =
                block_allocator_t::make(
                    // this memory will be cleaned up by the allocator
                    // TODO: optimize this... similar memory usage to stack
                    // allocator, although linked list nodes should be a perfect
                    // use case for block allocator
                    // TODO: look in to what is actually going on here...
                    // blocks in a block allocator dont' have extra space for
                    // bookkeeping, and all these allocations fit in the blocks,
                    // so this should be really solid memory usage
                    // I think it's due to the minimum block size of the block
                    // allocator being 32? although I'm not sure it would even
                    // work with smaller blocks
                    allo::alloc<uint8_t>(global_allocator, 1825).release(),
                    global_allocator, 16)
                    .release();
            tests::allocate_object_with_linked_list(ally);
        }

        SUBCASE("destruction callback and sometimes OOM on register, doesnt "
                "get called")
        {
            c_allocator_t global_allocator;
            uint8_t expected_called = 0;
            uint8_t called = 0;
            for (size_t i = 0; i < 5; ++i) {
                const auto blocksize = static_cast<size_t>(32 * std::pow(2, i));

                // oneshot allocator so that we OOM instead of realloc-ing
                auto oneshot =
                    oneshot_allocator_t::make_owned(
                        allo::alloc<uint8_t>(global_allocator, blocksize * 4)
                            .release(),
                        global_allocator)
                        .release();
                // 256 bytes per block, 32 byte aligned blocks
                auto ally =
                    block_allocator_t::make(oneshot.shoot(), oneshot, blocksize)
                        .release();

                auto callback = [](void *data) {
                    ++(*reinterpret_cast<uint8_t *>(data));
                };
                REQUIRE(ally.register_destruction_callback(callback, &called)
                            .okay());
                ++expected_called;
                REQUIRE(ally.register_destruction_callback(callback, &called)
                            .okay());
                ++expected_called;
                REQUIRE(ally.register_destruction_callback(callback, &called)
                            .okay());
                ++expected_called;
                REQUIRE(ally.register_destruction_callback(callback, &called)
                            .okay());
                ++expected_called;

                auto lasterr =
                    ally.register_destruction_callback(callback, &called).err();

                // if the blocks are 32 bytes in size, then we should oom on the
                // fifth time
                if (i == 0) {
                    REQUIRE(lasterr == AllocationStatusCode::OOM);
                } else {
                    ++expected_called;
                }
            }
            REQUIRE(called == expected_called);
        }
    }
}
