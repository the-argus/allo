#pragma once

#include "allo/abstracts.h"

namespace allo {

/// A oneshot allocator is effectively just a wrapper around a slice of memory,
/// treating that memory both as its only allocation and its entire buffer. That
/// means reallocation will always fail unless you are shrinking the allocation,
/// and freeing will always return Okay but will do nothing.
/// Useful for breaking a dependency chain between allocators.
class oneshot_allocator_t : private detail::dynamic_allocator_base_t
{
  private:
    struct M
    {
        zl::opt<DynamicHeapAllocatorRef> parent;
        zl::slice<uint8_t> mem;
        allocator_properties_t properties;
    } m;

    static zl::res<oneshot_allocator_t, AllocationStatusCode>
    make_inner(const zl::slice<uint8_t> &memory,
               zl::opt<DynamicHeapAllocatorRef> parent) noexcept;

  public:
    static constexpr detail::AllocatorType enum_value =
        detail::AllocatorType::OneshotAllocator;
    static constexpr uint8_t interfaces = 0b01111;

    template <typename Allocator>
    inline static zl::res<oneshot_allocator_t, AllocationStatusCode>
    make_owned(zl::slice<uint8_t> memory, Allocator &parent) noexcept
    {
        return make_inner(memory, DynamicHeapAllocatorRef(parent));
    }

    inline static zl::res<oneshot_allocator_t, AllocationStatusCode>
    make(zl::slice<uint8_t> memory) noexcept
    {
        return make_inner(memory, {});
    }

    [[nodiscard]] allocation_result_t
    realloc_bytes(zl::slice<uint8_t> mem, size_t old_typehash, size_t new_size,
                  size_t new_typehash) noexcept;

    allocation_status_t free_bytes(zl::slice<uint8_t> mem,
                                   size_t typehash) noexcept;

    [[nodiscard]] allocation_result_t alloc_bytes(size_t bytes,
                                                  uint8_t alignment_exponent,
                                                  size_t typehash) noexcept;

    [[nodiscard]] allocation_status_t
    free_status(zl::slice<uint8_t> mem, size_t typehash) const noexcept;

    [[nodiscard]] inline constexpr const allocator_properties_t &
    properties() const noexcept
    {
        return m.properties;
    }

    ~oneshot_allocator_t() noexcept;
    // cannot be copied
    oneshot_allocator_t(const oneshot_allocator_t &other) = delete;
    oneshot_allocator_t &operator=(const oneshot_allocator_t &other) = delete;
    // can be move constructed
    oneshot_allocator_t(oneshot_allocator_t &&other) noexcept;
    // but not move assigned
    oneshot_allocator_t &operator=(oneshot_allocator_t &&other) = delete;

    inline oneshot_allocator_t(M &&members) noexcept : m(members)
    {
        type = enum_value;
    }
};
} // namespace allo

#ifdef ALLO_HEADER_ONLY
#include "allo/impl/oneshot_allocator.h"
#endif
