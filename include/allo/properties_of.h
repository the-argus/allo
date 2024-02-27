#pragma once
#include "allo/allocator_interfaces.h"

namespace allo {
template <typename AllocatorOrInterface>
inline constexpr const allocator_properties_t&
properties_of(const AllocatorOrInterface &allocator) noexcept {
    return detail::memory_info_provider_t::_properties(std::addressof(allocator));
}
}