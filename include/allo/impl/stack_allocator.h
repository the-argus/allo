#pragma once
#ifndef ALLO_HEADER_ONLY
#ifndef ALLO_OVERRIDE_IMPL_INCLUSION_GUARD
#error \
    "Attempt to include allo/impl header file but header-only mode is not enabled."
#endif
#endif

#include "allo/stack_allocator.h"
#include <cmath>
#include <cstring>
#include <memory>
#include <ziglike/defer.h>
#include <ziglike/slice.h>

#ifdef ALLO_HEADER_ONLY
#ifndef ALLO_FUNC
#define ALLO_FUNC inline
#endif
#else
#define ALLO_FUNC
#endif

namespace allo {

// NOTE: this function is identical to scratch allocator
ALLO_FUNC stack_allocator_t::~stack_allocator_t() noexcept
{
    // call all destruction callbacks
    {
        destruction_callback_entry_t *iter = m.last_callback;
        while (iter) {
            iter->callback(iter->user_data);
            iter = iter->prev;
        }
    }

    // free if we are owning
    if (!m.parent.is_heap())
        return;

    if (m.blocks) {
        while (auto iter = m.blocks->end()) {
            m.parent.get_heap_unchecked().free_bytes(iter.value(), 0);
            m.blocks->pop();
        }
    } else {
        m.parent.get_heap_unchecked().free_bytes(m.memory, 0);
    }
}

// NOTE: this function is identical to scratch allocator
ALLO_FUNC
stack_allocator_t::stack_allocator_t(stack_allocator_t &&other) noexcept
    : m(other.m)
{
    m_type = other.m_type;
    other.m.parent = any_allocator_t();
    other.m.last_callback = nullptr;
}

ALLO_FUNC allocation_result_t stack_allocator_t::alloc_bytes(
    size_t bytes, uint8_t alignment_exponent, size_t typehash) noexcept
{
    void *res = raw_alloc(bytes, 1 << alignment_exponent);
    if (!res) {
        auto status = try_make_space_for_at_least(bytes, alignment_exponent);
        if (!status.okay()) [[unlikely]] {
            // NOTE: should we ignore errors here and just still try to alloc?
            return status.err();
        }
        res = raw_alloc(bytes, 1 << alignment_exponent);
    }
    return zl::raw_slice<uint8_t>(*reinterpret_cast<uint8_t *>(res), bytes);
}

ALLO_FUNC void *stack_allocator_t::raw_alloc(size_t align,
                                             size_t typesize) noexcept
{
    // these will get modified in place by std::align
    void *new_available_start = m.top;
    size_t new_size = m.memory.end().ptr() - m.top;
    if (std::align(align, typesize, new_available_start, new_size)) {
        if (new_available_start >= (m.memory.end().ptr() - typesize)) {
            return nullptr;
        }
        m.top = reinterpret_cast<uint8_t *>(new_available_start) + typesize;
        assert(zl::memcontains_one(m.memory, m.top));
        return new_available_start;
    }
    return nullptr;
}

ALLO_FUNC allocation_status_t stack_allocator_t::realloc() noexcept {}

ALLO_FUNC allocation_status_t
stack_allocator_t::free_status(bytes_t mem, size_t typehash) const noexcept
{
    return free_common(mem, typehash).err();
}

ALLO_FUNC zl::res<stack_allocator_t::previous_state_t &, AllocationStatusCode>
stack_allocator_t::free_common(bytes_t mem, size_t typehash) const noexcept
{
}

ALLO_FUNC allocation_status_t
stack_allocator_t::free_bytes(bytes_t mem, size_t typehash) noexcept
{
}

ALLO_FUNC allocation_result_t
stack_allocator_t::remap_bytes(bytes_t mem, size_t old_typehash,
                               size_t new_size, size_t new_typehash) noexcept
{
    if (old_typehash != m.last_type_hashcode)
        return AllocationStatusCode::InvalidArgument;

    // always err
    return AllocationStatusCode::OOM;
}

ALLO_FUNC stack_allocator_t
stack_allocator_t::make_inner(bytes_t memory, any_allocator_t parent) noexcept
{
#ifndef NDEBUG
    if (parent.is_heap()) {
        assert(parent.get_heap_unchecked().free_status(memory, 0).okay());
    }
#endif
    return M{
        .memory = memory,
        .top = memory.data(),
        .original_size = memory.size(),
        .parent = parent,
    };
}

// NOTE: this function is identical to scratch allocator
ALLO_FUNC allocation_status_t stack_allocator_t::register_destruction_callback(
    destruction_callback_t callback, void *user_data) noexcept
{
    if (!callback) {
        assert(callback);
        return AllocationStatusCode::InvalidArgument;
    }
    auto res = allo::alloc_one<destruction_callback_entry_t>(*this);
    if (!res.okay())
        return res.err();
    destruction_callback_entry_t &newentry = res.release();
    newentry = destruction_callback_entry_t{
        .callback = callback,
        .user_data = user_data,
        .prev = m.last_callback,
    };
    m.last_callback = &newentry;
    return AllocationStatusCode::Okay;
}
} // namespace allo
