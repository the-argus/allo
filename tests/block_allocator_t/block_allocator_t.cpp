#include "allo/block_allocator.h"
#include "allo/c_allocator.h"
#include "allo/typed_allocation.h"
#include "allo/typed_freeing.h"
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
        struct Parent;

        struct Child
        {
            int age;
            Parent *parent;
        };

        struct Parent
        {
            std::array<char, 80> name;
            size_t num_children = 0;
            std::array<Child, 4> *children;
            [[nodiscard]] inline zl::slice<Child> getChildren() const
            {
                return zl::slice<Child>(*children, 0, num_children);
            }

            static Parent &make_on_heap(DynamicHeapAllocatorRef allocator,
                                        const char *name)
            {
                Parent &parent =
                    allo::construct_one<Parent>(allocator).release();
                parent.children =
                    &allo::construct_one<std::array<Child, 4>>(allocator)
                         .release();
                // NOLINTNEXTLINE
                std::snprintf(parent.name.data(), parent.name.size(), "%s",
                              name);
                return parent;
            }
        };

        SUBCASE("parent/children example")
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

                Parent &parent1 = Parent::make_on_heap(ally, "Sharon");
                Parent &parent1_wife = Parent::make_on_heap(ally, "Leslie");
                Parent &parent2 = Parent::make_on_heap(ally, "Steve");
                REQUIRE(strcmp("Sharon", parent1.name.data()) == 0);
                REQUIRE(strcmp("Leslie", parent1_wife.name.data()) == 0);
                REQUIRE(strcmp("Steve", parent2.name.data()) == 0);

                free_one(ally, *parent1.children);
                parent1.children = nullptr;
            }

            // this memory will leak since we aren't using a real allocator
            Parent &parent1 = Parent::make_on_heap(global_allocator, "Sharon");
            Parent &parent1_wife =
                Parent::make_on_heap(global_allocator, "Leslie");
            Parent &parent2 = Parent::make_on_heap(global_allocator, "Steve");

            REQUIRE(strcmp("Sharon", parent1.name.data()) == 0);
            REQUIRE(strcmp("Leslie", parent1_wife.name.data()) == 0);
            REQUIRE(strcmp("Steve", parent2.name.data()) == 0);

            free_one(global_allocator, *parent1.children);
            parent1.children = nullptr;

            // silly usage of memcompare on cstrings. but hey its one time i can
            // actually use it heh
            REQUIRE(zl::memcompare(
                zl::slice<const char>(parent1.name, 0,
                                      strlen(parent1.name.data())),
                zl::raw_slice<const char>(*((char *)"Sharon"), 6)));
        }

        SUBCASE("OOM")
        {
            c_allocator_t global_allocator;

            {
                // 256 bytes per block, 32 byte aligned blocks
                auto ally =
                    block_allocator_t::make(
                        // this memory will be cleaned up by the allocator
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
                REQUIRE(five.err() == AllocationStatusCode::OOM);
            }
        }

        SUBCASE("destruction callback")
        {
            c_allocator_t global_allocator;
            uint8_t expected_called = 0;
            uint8_t called = 0;
            for (size_t i = 0; i < 5; ++i) {
                const auto blocksize = static_cast<size_t>(32 * std::pow(2, i));
                auto ally =
                    block_allocator_t::make(
                        // this memory will be cleaned up by the allocator
                        allo::alloc<uint8_t>(global_allocator, blocksize * 4)
                            .release(),
                        global_allocator, blocksize)
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
