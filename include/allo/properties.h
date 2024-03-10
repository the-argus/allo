#pragma once
#include "allo/detail/forward_decls.h"
#include <ziglike/opt.h>

namespace allo {
struct allocator_requirements_t
{
    // the largest single contiguous allocation you plan on making. null means
    // unbounded, so you require an allocator like malloc which can map virtual
    // memory.
    zl::opt<size_t> maximum_contiguous_bytes;
    // the largest alignment you will require from the allocator.
    uint8_t maximum_alignment = 8;
};

struct allocator_properties_t
{
  public:
    /// Check if the allocator properties meet some given requirements
    [[nodiscard]] inline constexpr bool
    meets(const allocator_requirements_t &requirements) const
    {
        if (!requirements.maximum_contiguous_bytes.has_value()) {
            if (m_maximum_contiguous_bytes != 0) {
                return false;
            }
        } else {
            if (requirements.maximum_contiguous_bytes.value() >
                m_maximum_contiguous_bytes) {
                return false;
            }
        }

        return m_maximum_alignment >= requirements.maximum_alignment;
    }

    /// Useful for testing, as a way of asserting that properties()
    /// getter works for a type and its upcasted reference
    inline constexpr friend bool
    operator==(const allocator_properties_t &a,
               const allocator_properties_t &b) noexcept
    {
        return a.m_maximum_alignment == b.m_maximum_alignment &&
               a.m_maximum_contiguous_bytes == b.m_maximum_contiguous_bytes;
    }

  private:
    allocator_properties_t(const allocator_properties_t &other) = default;
    allocator_properties_t &
    operator=(const allocator_properties_t &other) = default;
    allocator_properties_t(allocator_properties_t &&other) = default;
    allocator_properties_t &operator=(allocator_properties_t &&other) = default;
    // zero means theoretically limitless contiguous allocation is possible
    size_t m_maximum_contiguous_bytes;
    uint8_t m_maximum_alignment;

    // NOLINTNEXTLINE
    inline constexpr allocator_properties_t(size_t max_contiguous_bytes,
                                            uint8_t max_alignment)
        : m_maximum_contiguous_bytes(max_contiguous_bytes),
          m_maximum_alignment(max_alignment)
    {
    }

  public:
    friend class allo::c_allocator_t;
    friend class allo::stack_allocator_t;
    friend class allo::block_allocator_t;
    friend class allo::scratch_allocator_t;
    friend class allo::oneshot_allocator_t;
    friend class allo::heap_allocator_t;
};
} // namespace allo
