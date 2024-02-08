#pragma once

#include "algorithm.h"
#include "hash.h"
#include <cstddef>

// compile time string type

namespace allo::ctti::detail {
class cstring
{
  public:
    template <size_t N>
    constexpr cstring(const char (&str)[N]) : cstring{&str[0], N - 1}
    {
    }

    constexpr cstring(const char *begin, size_t length)
        : _str{begin}, _length{length}
    {
    }

    constexpr cstring(const char *begin, const char *end)
        : cstring{begin, static_cast<size_t>(end - begin)}
    {
    }

    constexpr cstring(const char *begin) : cstring{begin, length(begin)} {}

    /// Get the length of a string at compile-time
    [[nodiscard]] static constexpr size_t length(const char *str)
    {
        return *str ? 1 + length(str + 1) : 0;
    }

    [[nodiscard]] constexpr hash_t hash() const
    {
        return fnv1a_hash(length(), begin());
    }

    [[nodiscard]] constexpr size_t length() const { return _length; }

    [[nodiscard]] constexpr size_t size() const { return length(); }

    [[nodiscard]] constexpr const char *begin() const { return _str; }

    [[nodiscard]] constexpr const char *end() const { return _str + _length; }

    [[nodiscard]] constexpr char operator[](size_t i) const { return _str[i]; }

    [[nodiscard]] constexpr const char *operator()(size_t i) const
    {
        return _str + i;
    }

    [[nodiscard]] constexpr cstring operator()(size_t begin, size_t end) const
    {
        return {_str + begin, _str + end};
    }

    [[nodiscard]] constexpr cstring pad(size_t begin_offset,
                                        size_t end_offset) const
    {
        return operator()(begin_offset, size() - end_offset);
    }

  private:
    const char *_str;
    size_t _length;
};

constexpr bool operator==(const cstring &lhs, const cstring &rhs)
{
    return ctti::detail::equal_range(lhs.begin(), lhs.end(), rhs.begin(),
                                     rhs.end());
}

constexpr bool operator!=(const cstring &lhs, const cstring &rhs)
{
    return !(lhs == rhs);
}
} // namespace allo::ctti::detail
