#include "allo/random_allocation_registry.hpp"

namespace allo::random_alloc {
void register_allocation(interfaces::AllocationType type, void *block,
                         size_t size_in_bytes) TESTING_NOEXCEPT
{
    // TODO:
}

/// Unregister an allocation. Returns a status code since this debugger knows
/// more about memory usage than anyone invoking this function. Intended to
/// always return Statuscode::Okay in release mode.
interfaces::allocation_status_t
unregister_allocation(interfaces::AllocationType type, void *block,
                      size_t size_in_bytes) TESTING_NOEXCEPT
{
    // TODO:
    return interfaces::status_code_e::Okay;
}
} // namespace allo::random_alloc
