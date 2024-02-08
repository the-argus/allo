#pragma once
#include "ziglike/opt.h"
#include "ziglike/res.h"
#include "ziglike/slice.h"
#include "ziglike/status.h"
#include <cstddef>
#include <cstdint>
#include <cstdlib>

namespace allo {

using allocation_type_t = uint16_t;

enum class AllocationStatusCode : uint8_t
{
    Okay,
    ResultReleased,
    AlreadyFreed,
    OOM,
};

using allocation_status_t = zl::status<AllocationStatusCode>;

/// May either be a successfull allocation, or a status code failure. Check by
/// calling okay(), and if okay, call release() to get the allocated memory.
using allocation_result_t = zl::res<zl::slice<uint8_t>, AllocationStatusCode>;

struct allocator_requirements_t
{
    // the upper bound of possible memory usage that you forsee.
    // the default (null) means unbounded / only bounded by hardware,
    // so you will require an allocator like malloc.
    zl::opt<size_t> maximum_bytes;
    // the largest alignment you will require from the allocator.
    uint8_t maximum_alignment = 8;
};

class memory_info_provider_t;

struct allocator_properties_t
{
  private:
    // zero maximum bytes means theoretically limitless maximum bytes
    size_t m_maximum_bytes;
    uint8_t m_maximum_alignment;

  public:
    friend class memory_info_provider_t;
    /// Check if the allocator properties meet some given requirements
    [[nodiscard]] inline constexpr bool
    meets(const allocator_requirements_t &requirements) const
    {
        if (!requirements.maximum_bytes.has_value())
            return m_maximum_bytes == 0;

        return (requirements.maximum_bytes.value() <= m_maximum_bytes ||
                m_maximum_bytes == 0) &&
               m_maximum_alignment >= requirements.maximum_alignment;
    }
};

class memory_info_provider_t
{
    [[nodiscard]] const allocator_properties_t &properties() const;
};

class allocator_t : public virtual memory_info_provider_t
{
    /// Request an allocation for some number of bytes with some alignment, and
    /// providing the typehash. If a non-typed allocator, 0 will be supplied as
    /// the hash.
    allocation_result_t alloc(size_t bytes, size_t alignment, size_t typehash);

  public:
    template <typename T, uint8_t alignment = alignof(T)>
    inline zl::res<T &, AllocationStatusCode> create_undefined()
    {
        auto res = alloc(sizeof(T), alignment, 0);
        if (!res.okay())
            return res.err();
        auto mem = res.release();
        assert(sizeof(T) == mem.size());
        return *reinterpret_cast<T *>(mem.data());
    }

    template <typename T, uint8_t alignment = alignof(T)>
    inline zl::res<zl::slice<T>, AllocationStatusCode>
    alloc_undefined(size_t number)
    {
        alloc(sizeof(T) * number, alignment, 0);
    }

    template <typename T, typename... Args>
    inline zl::res<T &, AllocationStatusCode> create(Args &&...args);

    template <typename T, typename... Args>
    inline zl::res<zl::slice<T>, AllocationStatusCode> alloc(Args &&...args);
};

class stack_reallocator_t : public virtual memory_info_provider_t
{
    allocation_result_t realloc(zl::slice<uint8_t> mem, size_t new_size);
};

class reallocator_t : public virtual stack_reallocator_t
{};

class stack_freer_t
{
    allocation_status_t free(zl::slice<uint8_t> mem);
};

class freer_t : public virtual stack_freer_t
{};

namespace detail {

enum class AllocatorType : uint8_t
{
    HeapAllocator,
    CAllocator,
    BlockAllocator,
    ThreadsafeAllocator,
    SegmentedArrayBlockAllocator,
    StackAllocator,
    ScratchAllocator,
    ArenaAllocator,
};

class dynamic_allocator_base_t
{
  protected:
    AllocatorType type;
};

} // namespace detail
} // namespace allo
