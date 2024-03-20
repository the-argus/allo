#pragma once
#ifndef ALLO_HEADER_ONLY
#ifndef ALLO_OVERRIDE_IMPL_INCLUSION_GUARD
#error \
    "Attempt to include allo/impl header file but header-only mode is not enabled."
#endif
#endif

#include "allo/scratch_allocator.h"

#ifdef ALLO_HEADER_ONLY
#ifndef ALLO_FUNC
#define ALLO_FUNC inline
#endif
#else
#define ALLO_FUNC
#endif

namespace allo {

ALLO_FUNC allocation_result_t scratch_allocator_t::alloc_bytes(
    size_t bytes, uint8_t alignment_exponent, size_t) noexcept
{
    void *new_top = top();
    assert(m.memory.size() >= m.top);
    size_t remaining = m.memory.size() - m.top;
    if (std::align(1 << alignment_exponent, bytes, new_top, remaining)) {
        assert(static_cast<uint8_t *>(new_top) >= top());
        auto result = zl::raw_slice(*top(), bytes);
        m.top = result.end().ptr() - m.memory.data();
        assert(result.end().ptr() <= m.memory.end().ptr());
        return result;
    }
    return AllocationStatusCode::OOM;
}

ALLO_FUNC allocation_status_t
scratch_allocator_t::register_destruction_callback(
    destruction_callback_t callback, void *user_data) noexcept
{
    return AllocationStatusCode::InvalidArgument;
}

ALLO_FUNC const allocator_properties_t &scratch_allocator_t::properties() const
{
    return m.properties;
}

ALLO_FUNC zl::res<scratch_allocator_t, AllocationStatusCode>
scratch_allocator_t::make_inner(
    zl::slice<uint8_t> memory,
    zl::opt<detail::dynamic_heap_allocator_t> parent) noexcept
{
    return scratch_allocator_t{M{
        .memory = memory,
        .parent = parent,
        .properties = allocator_properties_t(memory.size(), 0),
        .top = 0,
    }};
}

ALLO_FUNC scratch_allocator_t::~scratch_allocator_t() noexcept
{
    if (m.parent) {
        m.parent.value().free_bytes(m.memory, 0);
    }
}

ALLO_FUNC
scratch_allocator_t::scratch_allocator_t(scratch_allocator_t &&other) noexcept
    : m(std::move(other.m))
{
    type = other.type;
    other.m.parent.reset();
}
} // namespace allo
