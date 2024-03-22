#pragma one
#include "allo/detail/abstracts.h"
#include "allo/status.h"

namespace allo {
template <typename Allocator, typename T>
allocation_status_t on_destroy(Allocator &allocator, T &allocated,
                               destruction_callback_t invocable) noexcept
{
    static_assert(
        detail::can_upcast<Allocator, detail::allocator_common_t>::type::value,
        "Cannot register a destruction callback with the given type.");
    static_assert(std::is_nothrow_destructible_v<T>,
                  "The given type is not nothrow destructible");
    return allocator.register_destruction_callback(invocable,
                                                   std::addressof(allocated));
}
} // namespace allo
