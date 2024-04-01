#pragma once

#ifndef ALLO_HEADER_ONLY
#ifndef ALLO_OVERRIDE_IMPL_INCLUSION_GUARD
#error \
    "Attempt to include allo/impl header file but header-only mode is not enabled."
#endif
#endif

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
    reservation_allocator_t &&other) noexcept
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
        size_t pages_needed = ((new_size - m.mem.size()) / m.pagesize) + 1;
        // add some read/write pages to this allocation
        int64_t res = mm_commit_pages(m.mem.end().ptr(), pages_needed);
        if (res != 0)
            return AllocationStatusCode::OOM;
        m.mem = zl::raw_slice(*m.mem.data(),
                              m.mem.size() + (pages_needed * m.pagesize));
    }
    return bytes_t(m.mem, 0, new_size);
}

ALLO_FUNC zl::res<reservation_allocator_t, AllocationStatusCode>
reservation_allocator_t::make(const options_t &options) noexcept
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
    int32_t commit_res = mm_commit_pages(reserve_res.data, options.committed);
    if (commit_res != 0) {
        return AllocationStatusCode::OOM;
    }
    unmap.cancel();
    return res<reservation_allocator_t, AllocationStatusCode>{
        std::in_place,
        M{
            .mem = raw_slice(*reinterpret_cast<uint8_t *>(reserve_res.data),
                             options.committed * pagesize_res.value),
            .pagesize = pagesize_res.value,
            .num_pages_reserved = max_pages,
            .properties =
                allocator_properties_t(pagesize_res.value * max_pages, 1),
        }};
}
} // namespace allo
