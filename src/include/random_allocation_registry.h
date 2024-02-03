#pragma once
/// This is a private header, intended only to be used by implementations of
/// interfaces::random_allocator_t.
///
/// Just a way of adding to some numbers which keep count of how much stuff is
/// allocated in different categories.

#include "allo/allocator_interfaces.hpp"

namespace allo::random_alloc {

/// Register an allocation. Intended to be a no-op in release mode.
/// Used for memory debugging and benchmarking.
void register_allocation(interfaces::AllocationType type, void *block,
                         size_t size_in_bytes) TESTING_NOEXCEPT;

/// Unregister an allocation. Returns a status code since this debugger knows
/// more about memory usage than anyone invoking this function. Intended to
/// always return Statuscode::Okay in release mode.
interfaces::allocation_status_t
unregister_allocation(interfaces::AllocationType type, void *block,
                      size_t size_in_bytes) TESTING_NOEXCEPT;
} // namespace allo::random_alloc
