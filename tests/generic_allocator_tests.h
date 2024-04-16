#pragma once
#include "allo.h"
#include "allo/reservation_allocator.h"
#include <cassert>

namespace allo::tests {
void allocate_object_with_linked_list(abstract_allocator_t &ally);
bytes_t large_allocation(abstract_allocator_t &ally, size_t maxpages);

template <typename Allocator, typename... Args>
void make_large_allocation_with(Args &&...args)
{
    constexpr size_t maxpages = 1000;
    reservation_allocator_t reserve =
        reservation_allocator_t::make(
            {.committed = 1, .additional_pages_reserved = maxpages})
            .release();
    static int count = 0;
    {
        auto ally = Allocator::make_owned(reserve.current_memory(), reserve,
                                          std::forward<Args>(args)...)
                        .release();
        ally.register_destruction_callback(
            [](void *data) {
                auto *d = ((int *)data);
                ++(*d);
            },
            &count);
        bytes_t result = allo::tests::large_allocation(ally, maxpages);
    }
    assert(count == 1);
}

// version of make_large_allocation_with where the allocator provides a make and
// make_owned which return the allocator, not a result
template <typename Allocator, typename... Args>
void make_large_allocation_with_nonfailing_make(Args &&...args)
{
    constexpr size_t maxpages = 1000;
    reservation_allocator_t reserve =
        reservation_allocator_t::make(
            {.committed = 1, .additional_pages_reserved = maxpages})
            .release();
    static int count = 0;
    {
        auto ally = Allocator::make_owned(reserve.current_memory(), reserve,
                                          std::forward<Args>(args)...);
        ally.register_destruction_callback(
            [](void *data) {
                auto *d = ((int *)data);
                ++(*d);
            },
            &count);
        bytes_t result = allo::tests::large_allocation(ally, maxpages);
    }
    assert(count == 1);
}

} // namespace allo::tests
