#pragma once

#include "detail/cstring.hpp"
#include "detail/name_filters.hpp"
#include "detail/pretty_function.hpp"
#include <cstdint>
#include <type_traits>

namespace allo::ctti {

/// Forward decls because we recurse between constexpr functions
template <typename T> constexpr allo::ctti::detail::cstring nameof();

namespace detail {

/// Typename length of arbitrary type T
template <typename T>
struct TypeNameLength
    : std::integral_constant<std::size_t, ctti::nameof<T>().length()>
{
};

} // namespace detail

namespace detail {

template <typename T> struct another_level_of_indirection
{
};

template <typename T, typename = void> struct nameof_impl
{
    static constexpr detail::cstring apply()
    {
        return ctti::detail::filter_typename_prefix(
            ctti::pretty_function::type<T>().pad(
                CTTI_TYPE_PRETTY_FUNCTION_LEFT,
                CTTI_TYPE_PRETTY_FUNCTION_RIGHT));
    }
};
} // namespace detail

template <typename T> constexpr ctti::detail::cstring nameof()
{
    using namespace ctti;
    return detail::nameof_impl<T>::apply();
}
} // namespace allo::ctti
