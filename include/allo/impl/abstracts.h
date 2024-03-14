#pragma once
#ifndef ALLO_HEADER_ONLY
#ifndef ALLO_OVERRIDE_IMPL_INCLUSION_GUARD
#error \
    "Attempt to include allo/impl header file but header-only mode is not enabled."
#endif
#endif

#include "allo/detail/abstracts.h"

#include "allo/block_allocator.h"
#include "allo/c_allocator.h"
#include "allo/heap_allocator.h"
#include "allo/oneshot_allocator.h"
#include "allo/reservation_allocator.h"
#include "allo/scratch_allocator.h"
#include "allo/stack_allocator.h"

#ifdef ALLO_HEADER_ONLY
#ifndef ALLO_FUNC
#define ALLO_FUNC inline
#endif
#else
#define ALLO_FUNC
#endif

namespace allo::detail {

template <typename T, typename... Args> struct get_properties_generic
{
    inline constexpr const allocator_properties_t &operator()(T *item)
    {
        return item->properties();
    }
};

template <typename T, typename... Args> struct alloc_bytes_generic
{
    inline allocation_result_t operator()(T *item, Args &&...args)
    {
        return item->alloc_bytes(std::forward<Args>(args)...);
    }
};

template <typename T, typename... Args> struct remap_bytes_generic
{
    inline allocation_result_t operator()(T *item, Args &&...args)
    {
        return item->remap_bytes(std::forward<Args>(args)...);
    }
};

template <typename T, typename... Args> struct free_bytes_generic
{
    inline allocation_status_t operator()(T *item, Args &&...args)
    {
        return item->free_bytes(std::forward<Args>(args)...);
    }
};

template <typename T, typename... Args> struct free_status_generic
{
    inline allocation_status_t operator()(T *item, Args &&...args)
    {
        return item->free_status(std::forward<Args>(args)...);
    }
};

template <typename T, typename... Args>
struct register_destruction_callback_generic
{
    inline allocation_status_t operator()(T *item, Args &&...args)
    {
        return item->register_destruction_callback(std::forward<Args>(args)...);
    }
};

template <template <typename, typename...> typename Callable, typename... Args>
auto return_from(dynamic_allocator_base_t *self, Args &&...args)
    -> decltype(Callable<c_allocator_t, Args...>{}(nullptr,
                                                   std::forward<Args>(args)...))
{
    switch (self->type) {
    case AllocatorType::CAllocator: {
        auto *c_alloc = reinterpret_cast<c_allocator_t *>(self);
        return Callable<c_allocator_t, Args...>{}(c_alloc,
                                                  std::forward<Args>(args)...);
    }
    case AllocatorType::BlockAllocator: {
        auto *block = reinterpret_cast<block_allocator_t *>(self);
        return Callable<block_allocator_t, Args...>{}(
            block, std::forward<Args>(args)...);
    }
    case AllocatorType::StackAllocator: {
        auto *stack = reinterpret_cast<stack_allocator_t *>(self);
        return Callable<stack_allocator_t, Args...>{}(
            stack, std::forward<Args>(args)...);
    }
    case AllocatorType::ScratchAllocator: {
        auto *scratch = reinterpret_cast<scratch_allocator_t *>(self);
        return Callable<scratch_allocator_t, Args...>{}(
            scratch, std::forward<Args>(args)...);
    }
    case AllocatorType::HeapAllocator: {
        auto *heap = reinterpret_cast<heap_allocator_t *>(self);
        return Callable<heap_allocator_t, Args...>{}(
            heap, std::forward<Args>(args)...);
    }
    case AllocatorType::OneshotAllocator: {
        auto *oneshot = reinterpret_cast<oneshot_allocator_t *>(self);
        return Callable<oneshot_allocator_t, Args...>{}(
            oneshot, std::forward<Args>(args)...);
    }
    case AllocatorType::ReservationAllocator: {
        auto *reservation = reinterpret_cast<reservation_allocator_t *>(self);
        return Callable<reservation_allocator_t, Args...>{}(
            reservation, std::forward<Args>(args)...);
    }
    default:
        // some sort of memory corruption going on
        std::abort();
    }
}

ALLO_FUNC const allocator_properties_t &
allocator_common_t::properties() const noexcept
{
    // the only thing that should inherit from this interface should also
    // inherit dynamic allocator base
    const auto *dyn_self =
        reinterpret_cast<const dynamic_allocator_base_t *>(ref);
    auto *mutable_self = const_cast<dynamic_allocator_base_t *>(dyn_self);
    return return_from<get_properties_generic>(mutable_self);
}

ALLO_FUNC allocation_result_t allocator_common_t::alloc_bytes(
    size_t bytes, uint8_t alignment_exponent, size_t typehash) noexcept
{
    auto *dyn_self = reinterpret_cast<dynamic_allocator_base_t *>(ref);
    return return_from<alloc_bytes_generic>(dyn_self, bytes, alignment_exponent,
                                            typehash);
}

ALLO_FUNC allocation_result_t dynamic_stack_allocator_t::remap_bytes(
    zl::slice<uint8_t> mem, size_t old_typehash, size_t new_size,
    size_t new_typehash) noexcept
{
    auto *dyn_self = reinterpret_cast<dynamic_allocator_base_t *>(ref);
    return return_from<remap_bytes_generic>(dyn_self, mem, old_typehash,
                                            new_size, new_typehash);
}

ALLO_FUNC allocation_status_t dynamic_stack_allocator_t::free_bytes(
    zl::slice<uint8_t> mem, size_t typehash) noexcept
{
    auto *dyn_self = reinterpret_cast<dynamic_allocator_base_t *>(ref);
    return return_from<free_bytes_generic>(dyn_self, mem, typehash);
}

ALLO_FUNC allocation_status_t dynamic_stack_allocator_t::free_status(
    zl::slice<uint8_t> mem, size_t typehash) noexcept
{
    auto *dyn_self = reinterpret_cast<const dynamic_allocator_base_t *>(ref);
    auto *mutable_self = const_cast<dynamic_allocator_base_t *>(dyn_self);
    return return_from<free_status_generic>(mutable_self, mem, typehash);
}

ALLO_FUNC allocation_status_t allocator_common_t::register_destruction_callback(
    destruction_callback_t callback, void *user_data) noexcept
{
    auto *dyn_self = reinterpret_cast<dynamic_allocator_base_t *>(ref);
    return return_from<register_destruction_callback_generic>(
        dyn_self, callback, user_data);
}
} // namespace allo::detail
