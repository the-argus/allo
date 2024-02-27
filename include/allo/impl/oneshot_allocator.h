#pragma once

#include "allo/allocator_interfaces.h"
#ifndef ALLO_HEADER_ONLY
#ifndef ALLO_OVERRIDE_IMPL_INCLUSION_GUARD
#error \
    "Attempt to include allo/impl header file but header-only mode is not enabled."
#endif
#endif

#include "allo/oneshot_allocator.h"

#ifdef ALLO_HEADER_ONLY
#ifndef ALLO_FUNC
#define ALLO_FUNC inline
#endif
#else
#define ALLO_FUNC
#endif

namespace allo {

ALLO_FUNC oneshot_allocator_t::~oneshot_allocator_t() noexcept
{
    if (m.parent.has_value()) {
        IFree::_free_bytes(std::addressof(m.parent.value()), m.mem, 0);
    }
}

ALLO_FUNC
oneshot_allocator_t::oneshot_allocator_t(oneshot_allocator_t &&other) noexcept
    : m(std::move(other.m))
{
    // the other one should no longer own the allocation, if it even did
    other.m.parent.reset();
    type = enum_value;
}

ALLO_FUNC zl::res<oneshot_allocator_t, AllocationStatusCode>
oneshot_allocator_t::make_inner(
    const zl::slice<uint8_t> &memory,
    zl::opt<allocator_with<IStackFree> &> parent) noexcept
{
    if (memory.size() == 0) {
        return AllocationStatusCode::InvalidArgument;
    }

    return zl::res<oneshot_allocator_t, AllocationStatusCode>(
        std::in_place, M{
                           parent,
                           memory,
                           make_properties(memory.size(), 1),
                       });
}

[[nodiscard]] ALLO_FUNC allocation_result_t oneshot_allocator_t::realloc_bytes(
    zl::slice<uint8_t> mem, size_t /*old_typehash*/, size_t new_size,
    size_t /*new_typehash*/) noexcept
{
    if (mem.data() != m.mem.data() || mem.size() > m.mem.size()) {
        return AllocationStatusCode::MemoryInvalid;
    }

    if (new_size > m.mem.size()) {
        return AllocationStatusCode::OOM;
    }

    return zl::slice<uint8_t>(m.mem, 0, new_size);
}

ALLO_FUNC allocation_status_t oneshot_allocator_t::free_bytes(
    zl::slice<uint8_t> /*mem*/, size_t /*typehash*/) noexcept
{
    return AllocationStatusCode::Okay;
}
[[nodiscard]] ALLO_FUNC allocation_status_t oneshot_allocator_t::free_status(
    zl::slice<uint8_t> /*mem*/, size_t /*typehash*/) const noexcept
{
    return AllocationStatusCode::Okay;
}

} // namespace allo
