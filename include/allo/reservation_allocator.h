#pragma once

#include "allo/detail/abstracts.h"

namespace allo {
class reservation_allocator_t : private detail::dynamic_allocator_base_t
{
  private:
    struct M
    {
        zl::slice<uint8_t> mem; // committed memory
        size_t pagesize;
        // num_pages_reserved is the maximum number of pages that this
        // allocation can be remapped t
        // NOTE: num_pages_reserved is sort of
        // encoded in the allocator properties already with
        // max_contigious_bytes. could remove num_pages_reserved and just
        // calculate it from properties instead.
        size_t num_pages_reserved;
        allocator_properties_t properties;
    } m;

  public:
    static constexpr detail::AllocatorType enum_value =
        detail::AllocatorType::ReservationAllocator;

    struct options_t
    {
        size_t committed;
        // extra pages past the end of the committed memory that we can expand
        // the allocation into
        size_t additional_pages_reserved;
        void *hint = (void *)0x800000000000;
    };

    /// Create a new reservation of a given size, with a number of additional
    /// pages reserved. Get the memory out of the reservation_allocator_t by
    /// calling current_memory().
    [[nodiscard]] static zl::res<reservation_allocator_t, AllocationStatusCode>
    make(const options_t &options) noexcept;

    /// Increase the size of the memory owned by this allocation without
    /// invalidating addresses.
    [[nodiscard]] allocation_result_t remap_bytes(zl::slice<uint8_t> mem,
                                                  size_t old_typehash,
                                                  size_t new_size,
                                                  size_t new_typehash) noexcept;

    /// Always invalid to try and free anything from a reservation.
    inline allocation_status_t free_bytes(zl::slice<uint8_t> mem, // NOLINT
                                          size_t /*typehash*/) noexcept
    {
        return free_status(mem, 0);
    }

    /// Always returns OOM. reservation allocator can only allocate once.
    [[nodiscard]] inline allocation_result_t
    alloc_bytes(size_t /*bytes*/, uint8_t /*alignment_exponent*/, // NOLINT
                size_t /*typehash*/) noexcept
    {
        return AllocationStatusCode::OOM;
    }

    /// Will return Okay if you pass in the slice of memory that this allocation
    /// owns (typehash is ignored). Otherwise, it returns MemoryInvalid.
    [[nodiscard]] inline allocation_status_t free_status( // NOLINT
        zl::slice<uint8_t> mem, size_t) const noexcept
    {
        if (mem.data() == m.mem.data())
            return AllocationStatusCode::Okay;
        return AllocationStatusCode::InvalidArgument;
    }

    /// Cannot register destruction callback for a single memory reservation.
    /// Nest another allocator inside it to achieve this.
    inline allocation_status_t
    register_destruction_callback(destruction_callback_t, void *) // NOLINT
        noexcept
    {
        return AllocationStatusCode::OOM;
    }

    [[nodiscard]] inline constexpr const allocator_properties_t &
    properties() const noexcept
    {
        return m.properties;
    }

    // return a slice to the memory currently allocated by this allocator.
    [[nodiscard]] inline constexpr zl::slice<uint8_t>
    current_memory() const noexcept
    {
        return m.mem;
    }

    ~reservation_allocator_t() noexcept;
    // cannot be copied
    reservation_allocator_t(const reservation_allocator_t &other) = delete;
    reservation_allocator_t &
    operator=(const reservation_allocator_t &other) = delete;
    // can be move constructed
    reservation_allocator_t(reservation_allocator_t &&other) noexcept;
    // but not move assigned
    reservation_allocator_t &
    operator=(reservation_allocator_t &&other) = delete;

    inline reservation_allocator_t(M &&members) noexcept : m(members)
    {
        type = enum_value;
    }
};
} // namespace allo

#ifdef ALLO_HEADER_ONLY
#include "allo/impl/reservation_allocator.h"
#endif
