#pragma once
#include "allo.h"
#include "allo/reservation_allocator.h"

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
    auto ally = Allocator::make_owned(reserve.current_memory(), reserve,
                                      std::forward<Args>(args)...)
                    .release();
    bytes_t result = allo::tests::large_allocation(ally, maxpages);
}
} // namespace allo::tests
