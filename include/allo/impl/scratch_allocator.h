#pragma once
#include <memory>
#include <ziglike/stdmem.h>
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

ALLO_FUNC allocation_status_t scratch_allocator_t::try_make_space_for_at_least(
    size_t bytes, uint8_t alignment_exponent) noexcept
{
    if (!m.parent)
        return AllocationStatusCode::OOM;

    size_t aligned_data =
        ((reinterpret_cast<size_t>(m.available_memory.data()) >>
          alignment_exponent) +
         1)
        << alignment_exponent;

    size_t new_size =
        m.memory.size() + bytes +
        (aligned_data - reinterpret_cast<size_t>(m.available_memory.data()));
    // round up to be an even integer exponential of the original allocation
    // size
    auto new_size_rounded = static_cast<size_t>(std::pow(
        m.original_size, std::ceil(std::log10(new_size) * m.remap_divisor)));
    assert(new_size_rounded > new_size);
    auto res = m.parent.value().remap_bytes(m.memory, 0, new_size_rounded, 0);
    if (!res.okay())
        return res.err();
    m.memory = res.release();
    if (m.available_memory.end().ptr() == m.memory.end().ptr())
        return AllocationStatusCode::Okay;
    // copy the stuff at the end of memory to the NEW end of memory
    assert(m.memory.size() > m.available_memory.size());
    bytes_t new_destruction_callbacks_mem = bytes_t(
        m.memory, m.memory.size() - m.available_memory.size(), m.memory.size());
    zl::memcopy(new_destruction_callbacks_mem, m.available_memory);
    return AllocationStatusCode::Okay;
}

ALLO_FUNC allocation_result_t scratch_allocator_t::alloc_bytes(
    size_t bytes, uint8_t alignment_exponent, size_t) noexcept
{
    auto tryalloc = [this](size_t bytes,
                           uint8_t alignment_exponent) -> allocation_result_t {
        void *new_top = m.available_memory.data();
        size_t remaining = m.available_memory.size();
        assert(detail::nearest_alignment_exponent(1 << alignment_exponent) ==
               alignment_exponent);
        if (std::align(1 << alignment_exponent, bytes, new_top, remaining)) {
            assert(remaining >= bytes);
            auto result =
                zl::raw_slice(*static_cast<uint8_t *>(new_top), bytes);
            assert(zl::memcontains(m.available_memory, result));
            m.available_memory =
                zl::raw_slice<uint8_t>(*result.end().ptr(), remaining - bytes);
            assert(m.available_memory.end().ptr() <= m.memory.end().ptr());
            return result;
        }
        return AllocationStatusCode::OOM;
    };
    auto res = tryalloc(bytes, alignment_exponent);
    if (!res.okay()) {
        try_make_space_for_at_least(bytes, alignment_exponent);
        return tryalloc(bytes, alignment_exponent);
    }
    return res;
}

ALLO_FUNC allocation_status_t
scratch_allocator_t::register_destruction_callback(
    destruction_callback_t callback, void *user_data) noexcept
{
    // TODO: make some sort of "common" function, maybe templated, between this
    // and stack allocator.
    // TODO: try to remap if we are owning
    if (!callback) {
        return AllocationStatusCode::InvalidArgument;
    }
    auto try_register = [this](destruction_callback_t callback,
                               void *user_data) -> allocation_status_t {
        size_t dummy_size = m.available_memory.size();
        void *head = m.available_memory.end().ptr();
        if (auto *aligned = (destruction_callback_entry_t *)std::align(
                alignof(destruction_callback_entry_t),
                sizeof(destruction_callback_entry_t), head, dummy_size)) {
            while ((void *)(aligned + 1) > m.available_memory.end().ptr()) {
                --aligned;
            }

            if ((void *)aligned < (void *)m.available_memory.data()) {
                return AllocationStatusCode::OOM;
            }

            m.available_memory = bytes_t(
                m.available_memory, 0,
                (uint8_t *)aligned - (uint8_t *)m.available_memory.data());

            *aligned = {callback, user_data};
            return AllocationStatusCode::Okay;
        };
        return AllocationStatusCode::OOM;
    };
    auto res = try_register(callback, user_data);
    if (!res.okay()) {
        try_make_space_for_at_least(
            sizeof(destruction_callback_entry_t),
            detail::alignment_exponent(alignof(destruction_callback_entry_t)));
        return try_register(callback, user_data);
    }
    return res;
}

ALLO_FUNC const allocator_properties_t &scratch_allocator_t::properties() const
{
    return m.properties;
}

ALLO_FUNC scratch_allocator_t scratch_allocator_t::make_inner(
    bytes_t memory,
    zl::opt<detail::abstract_heap_allocator_t &> parent) noexcept
{
    return scratch_allocator_t{M{
        .memory = memory,
        .available_memory = memory,
        .parent = parent,
        .properties = allocator_properties_t(memory.size(), 0),
        .original_size = memory.size(),
        .remap_divisor = static_cast<float>(1.0 / std::log10(memory.size())),
    }};
}

ALLO_FUNC scratch_allocator_t::~scratch_allocator_t() noexcept
{
    // call all destruction callbacks
    auto *entry = reinterpret_cast<destruction_callback_entry_t *>(
        m.available_memory.end().ptr());
    while ((uint8_t *)(entry + 1) <= m.memory.end().ptr()) {
        entry->callback(entry->user_data);
        ++entry;
    }

    // free if we are owning
    if (m.parent) {
        m.parent.value().free_bytes(m.memory, 0);
    }
}

ALLO_FUNC
scratch_allocator_t::scratch_allocator_t(scratch_allocator_t &&other) noexcept
    : m(std::move(other.m))
{
    m_type = other.m_type;
    other.m.parent.reset();
}
} // namespace allo
