#pragma once
#include "allo/detail/abstracts.h"
#include <type_traits>

namespace allo::detail {

template <typename T>
inline constexpr bool is_threadsafe =
    std::is_base_of_v<abstract_threadsafe_heap_allocator_t, T>;

} // namespace allo::detail
