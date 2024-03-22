#include "heap_tests.h"
#include "allo.h"
#include "allo/rtti.h"
#include "doctest.h"
#include "ziglike/stdmem.h"
using namespace allo;

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

    static Parent &make_on_heap(HeapAllocatorDynRef allocator, const char *name)
    {
        Parent &parent = allo::construct_one<Parent>(allocator).release();
        parent.children =
            &allo::construct_one<std::array<Child, 4>>(allocator).release();
        // NOLINTNEXTLINE
        std::snprintf(parent.name.data(), parent.name.size(), "%s", name);
        return parent;
    }
};

namespace allo::tests {
void allocate_480_bytes_related_objects(HeapAllocatorDynRef heap)
{
    Parent &parent1 = Parent::make_on_heap(heap, "Sharon");
    Parent &parent1_wife = Parent::make_on_heap(heap, "Leslie");
    Parent &parent2 = Parent::make_on_heap(heap, "Steve");

    REQUIRE(strcmp("Sharon", parent1.name.data()) == 0);
    REQUIRE(strcmp("Leslie", parent1_wife.name.data()) == 0);
    REQUIRE(strcmp("Steve", parent2.name.data()) == 0);

    // TODO: add free overload that accepts a pointer by reference and
    // sets it to nullptr
    free_one(heap, *parent1.children);
    parent1.children = nullptr;

    // silly usage of memcompare on cstrings. but hey its one time i can
    // actually use it heh
    REQUIRE(zl::memcompare(
        zl::slice<const char>(parent1.name, 0, strlen(parent1.name.data())),
        zl::raw_slice<const char>(*((char *)"Sharon"), 6)));
}

void typed_alloc_realloc_free(HeapAllocatorDynRef heap)
{
    struct Test
    {
        int id;
        bool active = false;
    };
    if (!heap.properties().meets(allocator_requirements_t{
            .maximum_contiguous_bytes = sizeof(Test) * 8,
            .maximum_alignment = alignof(Test),
        })) {
        printf("[WARN] Skipping test %s for allocator of type %s\n",
               __PRETTY_FUNCTION__, allocator_typename(heap));
        // dont run the tests if the allocator is going to OOM
        return;
    }
    static_assert(detail::nearest_alignment_exponent(alignof(Test)) <= 5);
    zl::slice<Test> first = allo::alloc<Test>(heap, 1).release();
    first = allo::realloc(heap, first, 8).release();
    first = allo::realloc(heap, first, 1).release();
    allo::free(heap, first);
}
} // namespace allo::tests
