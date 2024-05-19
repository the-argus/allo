#pragma once
#include "allo.h"
#include "allo/reservation_allocator.h"
#include <cassert>

namespace allo::tests {
void allocate_object_with_linked_list(abstract_allocator_t& ally);
bytes_t large_allocation(abstract_allocator_t& ally, size_t maxpages);

// version of make_large_allocation_with where the allocator provides a make and
// make_owning which return the allocator, not a result
template <typename Allocator, typename... Args>
void make_large_allocation_with_nonfailing_make(Args&&... args)
{
    constexpr size_t maxpages = 1024;
    constexpr size_t pages_to_allocate = 1000;
    reservation_allocator_t reserve =
        reservation_allocator_t::make(
            {.committed = 1, .additional_pages_reserved = maxpages})
            .release();
    static int count = 0;
    {
        auto ally = Allocator::make_owning(reserve.current_memory(), reserve,
                                           std::forward<Args>(args)...);
        ally.register_destruction_callback(
            [](void* data) {
                auto* d = ((int*)data);
                ++(*d);
            },
            &count);
        bytes_t result = allo::tests::large_allocation(ally, pages_to_allocate);
    }
    assert(count == 1);
}

template <typename Allocator>
void allocate_free_then_allocate_again(
    detail::abstract_stack_allocator_t& stack)
{
    auto initial_mem = alloc<uint8_t>(stack, 500).release();
    auto ally = Allocator::make(initial_mem, stack);

    struct node_t
    {
        zl::opt<node_t&> left;
        zl::opt<node_t&> right;
        struct
        {
            int item = 0;
            std::array<char, 80> name = {0};
        } payload;
    };

    node_t& one = construct_one<node_t>(ally).release();
    assert(!one.right);
    assert(!one.left);

    one = node_t{
        .left = construct_one<node_t>(ally).release(),
        .right = construct_one<node_t>(ally).release(),
    };

    assert(one.left);
    assert(one.right);

    one.left.value().left = construct_one<node_t>(ally).release();
    one.left.value().right = construct_one<node_t>(ally).release();
    one.right.value().left = construct_one<node_t>(ally).release();
    one.right.value().right = construct_one<node_t>(ally).release();

    one.left.value().left.value().left = construct_one<node_t>(ally).release();
    one.left.value().left.value().right = construct_one<node_t>(ally).release();
    one.left.value().right.value().left = construct_one<node_t>(ally).release();
    one.left.value().right.value().right =
        construct_one<node_t>(ally).release();
    one.right.value().left.value().left = construct_one<node_t>(ally).release();
    one.right.value().left.value().right =
        construct_one<node_t>(ally).release();
    one.right.value().right.value().left =
        construct_one<node_t>(ally).release();
    one.right.value().right.value().right =
        construct_one<node_t>(ally).release();

    one.payload.item = 0;
    one.left.value().payload.item = 1;
    one.right.value().payload.item = 2;
    one.left.value().left.value().payload.item = 3;
    one.right.value().left.value().payload.item = 4;
    one.left.value().right.value().payload.item = 5;
    one.right.value().right.value().payload.item = 6;
    one.left.value().left.value().left.value().payload.item = 7;
    one.left.value().left.value().right.value().payload.item = 8;
    one.left.value().right.value().left.value().payload.item = 9;
    one.left.value().right.value().right.value().payload.item = 10;
    one.right.value().left.value().left.value().payload.item = 11;
    one.right.value().left.value().right.value().payload.item = 12;
    one.right.value().right.value().left.value().payload.item = 13;
    one.right.value().right.value().right.value().payload.item = 14;

    std::strcpy(one.payload.name.data(), "head node");
    std::strcpy(one.left.value().payload.name.data(), "left node");
    std::strcpy(one.right.value().payload.name.data(), "right node");

    // these get freed in the opposite order they were created, in order to
    // work with stack allocator
    free_one(ally, one.right.value().right.value().right.value());
    free_one(ally, one.right.value().right.value().left.value());
    free_one(ally, one.right.value().left.value().right.value());
    free_one(ally, one.right.value().left.value().left.value());
    free_one(ally, one.left.value().right.value().right.value());
    free_one(ally, one.left.value().right.value().left.value());
    free_one(ally, one.left.value().left.value().right.value());
    free_one(ally, one.left.value().left.value().left.value());

    free_one(ally, one.right.value().right.value());
    free_one(ally, one.right.value().left.value());
    free_one(ally, one.left.value().right.value());
    free_one(ally, one.left.value().left.value());

    // one big contiguous buffer just to mess with stuff and see what happens
    zl::slice<uint8_t> buffer = alloc<uint8_t>(ally, 10000).release();

    node_t* iter = &one.right.value();
    for (size_t i = 0; i < 100; ++i) {
        iter->left = construct_one<node_t>(ally).release();
        iter->left.value().payload.item = (int)i;
        iter = &iter->left.value();
    }
}

} // namespace allo::tests
