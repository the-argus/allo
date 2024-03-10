#pragma once

#include "allo/detail/abstracts.h"

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
        zl::opt<detail::dynamic_heap_allocator_t> parent;
        zl::slice<uint8_t> mem;
        allocator_properties_t properties;
    } m;

    static zl::res<oneshot_allocator_t, AllocationStatusCode>
    make_inner(const zl::slice<uint8_t> &memory,
               zl::opt<detail::dynamic_heap_allocator_t> parent) noexcept;

  public:
    static constexpr detail::AllocatorType enum_value =
        detail::AllocatorType::OneshotAllocator;

    /// Make a oneshot allocator which takes ownership of some memory. It will
    /// ask the given parent allocator to free the given memory when it is
    /// destroyed.
    template <typename Allocator>
    inline static zl::res<oneshot_allocator_t, AllocationStatusCode>
    make_owned(zl::slice<uint8_t> &&memory, Allocator &parent) noexcept
    {
        return make_inner(memory, detail::dynamic_heap_allocator_t(parent));
    }

    /// Return a oneshot allocator that does not try to free its memory when
    /// it is destroyed.
    inline static zl::res<oneshot_allocator_t, AllocationStatusCode>
    make(zl::slice<uint8_t> memory) noexcept
    {
        return make_inner(memory, {});
    }

    /// Returns OOM or MemoryInvalid, always.
    [[nodiscard]] allocation_result_t remap_bytes(zl::slice<uint8_t> mem,
                                                  size_t old_typehash,
                                                  size_t new_size,
                                                  size_t new_typehash) noexcept;

    /// Identical to free_status. There's no need for this allocator to do
    /// anything on free because it only keeps track of one allocation and you
    /// can't do anything like realloc it anyways.
    allocation_status_t free_bytes(zl::slice<uint8_t> mem,
                                   size_t typehash) noexcept;

    /// Always  returns OOM with a oneshot allocator, to simulate an actual
    /// heap OOM.
    [[nodiscard]] allocation_result_t alloc_bytes(size_t bytes,
                                                  uint8_t alignment_exponent,
                                                  size_t typehash) noexcept;

    /// Returns Okay if the memory you are trying to free is the memory returned
    /// by shoot(), otherwise it returns MemoryInvalid.
    [[nodiscard]] allocation_status_t
    free_status(zl::slice<uint8_t> mem, size_t typehash) const noexcept;

    inline allocation_status_t
    register_destruction_callback(destruction_callback_t /*callback*/,
                                  void * /*user_data*/) noexcept
    {
        return AllocationStatusCode::OOM;
    }

    [[nodiscard]] inline constexpr const allocator_properties_t &
    properties() const noexcept
    {
        return m.properties;
    }

    /// Return the memory owned by this allocator, the only thing which is okay
    /// to free
    [[nodiscard]] inline constexpr zl::slice<uint8_t> shoot() const noexcept
    {
        return m.mem;
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
