#pragma once

#ifndef ALLO_HEADER_ONLY
#ifndef ALLO_OVERRIDE_IMPL_INCLUSION_GUARD
#error \
    "Attempt to include allo/impl header file but header-only mode is not enabled."
#endif
#endif

#include "allo/detail/asserts.h"
#include "allo/memory_map.h"
#include "allo/reservation_allocator.h"
#include <ziglike/defer.h>

#ifdef ALLO_HEADER_ONLY
#ifndef ALLO_FUNC
#define ALLO_FUNC inline
#endif
#else
#define ALLO_FUNC
#endif

namespace allo {
ALLO_FUNC reservation_allocator_t::~reservation_allocator_t() noexcept
{
    mm_memory_unmap(m.mem.data(), m.num_pages_reserved * m.pagesize);
}

ALLO_FUNC reservation_allocator_t::reservation_allocator_t(
    reservation_allocator_t&& other) noexcept
    : m(other.m)
{
    m_type = enum_value;
}

ALLO_FUNC allocation_result_t reservation_allocator_t::remap_bytes(
    bytes_t mem, size_t, size_t new_size, size_t) noexcept
{
    if (mem.data() != m.mem.data()) {
        return AllocationStatusCode::MemoryInvalid;
    }

    // requires new pages which are not committed
    if (new_size > m.mem.size()) {
        const auto pages_needed = static_cast<size_t>(
            std::ceil(static_cast<double>(new_size - m.mem.size()) /
                      static_cast<double>(m.pagesize)));
        const size_t current_pages = m.mem.size() / m.pagesize;
        ALLO_INTERNAL_ASSERT(m.mem.size() % m.pagesize == 0);
        const size_t total_pages = pages_needed + current_pages;
        // catch error before it happens
        if (total_pages > m.num_pages_reserved)
            return AllocationStatusCode::OOM;
        // add some read/write pages to this allocation
        int64_t res = mm_commit_pages(m.mem.data(), total_pages);
        if (res != 0)
            return AllocationStatusCode::OOM;
        m.mem = zl::raw_slice(*m.mem.data(), total_pages * m.pagesize);
    }
    return bytes_t(m.mem, 0, new_size);
}

ALLO_FUNC zl::res<reservation_allocator_t, AllocationStatusCode>
reservation_allocator_t::make(const options_t& options) noexcept
{
    using namespace zl;
    auto pagesize_res = mm_get_page_size();
    if (!pagesize_res.has_value) {
        return AllocationStatusCode::OsErr;
    }
    size_t max_pages = options.committed + options.additional_pages_reserved;
    auto reserve_res = mm_reserve_pages(options.hint, max_pages);
    if (reserve_res.code != 0) {
        return AllocationStatusCode::OOM;
    }
    defer unmap([&reserve_res]() {
        mm_memory_unmap(reserve_res.data, reserve_res.bytes);
    });
    int64_t commit_res = mm_commit_pages(reserve_res.data, options.committed);
    if (commit_res != 0) {
        return AllocationStatusCode::OOM;
    }
    unmap.cancel();
    return res<reservation_allocator_t, AllocationStatusCode>{
        std::in_place,
        M{
            .mem = raw_slice(*reinterpret_cast<uint8_t*>(reserve_res.data),
                             options.committed * pagesize_res.value),
            .pagesize = pagesize_res.value,
            .num_pages_reserved = max_pages,
        }};
}
} // namespace allo
