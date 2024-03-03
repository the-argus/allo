#include "allo/block_allocator.h"
#include "allo/c_allocator.h"
#include "allo/typed_allocation.h"
#include "allo/typed_freeing.h"
#include "test_header.h"

#include <ziglike/stdmem.h>

using namespace allo;

TEST_SUITE("block_allocator_t")
{
    TEST_CASE("Construction and type behavior")
    {
        SUBCASE("Default construction")
        {
            c_allocator_t global_allocator;
            auto initial_memory =
                allo::alloc<uint8_t>(global_allocator, 2000).release();
            auto mally = block_allocator_t::make(initial_memory,
                                                 global_allocator, 200, 2);
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
                auto initial_memory =
                    allo::alloc<uint8_t>(global_allocator, 2000).release();
                auto ally = block_allocator_t::make(initial_memory,
                                                    global_allocator, 200, 2)
                                .release();

                Parent &parent1 = Parent::make_on_heap(ally, "Sharon");
                Parent &parent1_wife = Parent::make_on_heap(ally, "Leslie");
                Parent &parent2 = Parent::make_on_heap(ally, "Steve");
                // have to free this because c allocator has no way of knowing
                // what it has freed and what it has not
                allo::free(global_allocator, initial_memory);
                REQUIRE(strcmp("Sharon", parent1.name.data()) == 0);
                REQUIRE(strcmp("Leslie", parent1_wife.name.data()) == 0);
                REQUIRE(strcmp("Steve", parent2.name.data()) == 0);
            }

            // this memory will leak since we aren't using a real allocator
            Parent &parent1 = Parent::make_on_heap(global_allocator, "Sharon");
            Parent &parent1_wife =
                Parent::make_on_heap(global_allocator, "Leslie");
            Parent &parent2 = Parent::make_on_heap(global_allocator, "Steve");

            REQUIRE(strcmp("Sharon", parent1.name.data()) == 0);
            REQUIRE(strcmp("Leslie", parent1_wife.name.data()) == 0);
            REQUIRE(strcmp("Steve", parent2.name.data()) == 0);

            // silly usage of memcompare on cstrings. but hey its one time i can
            // actually use it heh
            REQUIRE(zl::memcompare(
                zl::slice<const char>(parent1.name),
                zl::raw_slice<const char>(*((char *)"Sharon"), 6)));
        }
    }
}
