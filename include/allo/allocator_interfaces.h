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
    // private memory inside allocator (bookkeeping data) has been overwritten
    // or it unexpected values
    Corruption,
    // invalid item trying to be freed, usually
    InvalidArgument,
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
    // the largest single contiguous allocation you plan on making. null means
    // unbounded, so you require an allocator like malloc which can map virtual
    // memory.
    zl::opt<size_t> maximum_contiguous_bytes;
    // the largest alignment you will require from the allocator.
    uint8_t maximum_alignment = 8;
};
namespace detail {
class memory_info_provider_t;
}

struct allocator_properties_t
{
  private:
    // zero maximum bytes means theoretically limitless maximum bytes
    size_t m_maximum_bytes;
    // zero means theoretically limitless contiguous allocation is possible
    size_t m_maximum_contiguous_bytes;
    uint8_t m_maximum_alignment;

  public:
    friend class memory_info_provider_t;
    /// Check if the allocator properties meet some given requirements
    [[nodiscard]] inline constexpr bool
    meets(const allocator_requirements_t &requirements) const
    {
        if (!requirements.maximum_bytes.has_value()) {
            if (m_maximum_bytes != 0) {
                return false;
            }
        } else {
            if (requirements.maximum_bytes.value() > m_maximum_bytes) {
                return false;
            }
        }

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
};

class heap_allocator_t;
class c_allocator_t;
class stack_allocator_t;
class segmented_array_block_allocator_t;
class block_allocator_t;
class scratch_allocator_t;
class region_allocator_t;

namespace detail {

class memory_info_provider_t
{
    [[nodiscard]] const allocator_properties_t &properties() const;
};

class allocator_interface_t
{};

class allocator_t : public memory_info_provider_t, public allocator_interface_t
{
  public:
    /// Request an allocation for some number of bytes with some alignment, and
    /// providing the typehash. If a non-typed allocator, 0 can be supplied as
    /// the hash.
    [[nodiscard]] allocation_result_t
    alloc_bytes(size_t bytes, size_t alignment, size_t typehash);
};

class stack_reallocator_t : public memory_info_provider_t,
                            public allocator_interface_t
{
    [[nodiscard]] allocation_result_t
    realloc_bytes(zl::slice<uint8_t> mem, size_t new_size, size_t typehash);
};

class reallocator_t : public stack_reallocator_t
{};

class stack_freer_t : public allocator_interface_t
{
    allocation_status_t free_bytes(zl::slice<uint8_t> mem, size_t typehash);
};

class freer_t : public stack_freer_t
{};

enum class AllocatorType : uint8_t
{
    HeapAllocator,
    CAllocator,
    BlockAllocator,
    SegmentedArrayBlockAllocator,
    StackAllocator,
    ScratchAllocator,
    // allocator created by arena. arena isnt actually an allocator, its more
    // like an allocator-allocator
    RegionAllocator,
    MAX_ALLOCATOR_TYPE
};

// clang-format off
class heap_allocator_enum_val_t { static constexpr AllocatorType allocator_type = AllocatorType::HeapAllocator; };
class c_allocator_enum_val_t { static constexpr AllocatorType allocator_type = AllocatorType::CAllocator; };
class block_allocator_enum_val_t { static constexpr AllocatorType allocator_type = AllocatorType::BlockAllocator; };
class segmented_array_block_allocator_enum_val_t { static constexpr AllocatorType allocator_type = AllocatorType::SegmentedArrayBlockAllocator; };
class stack_allocator_enum_val_t { static constexpr AllocatorType allocator_type = AllocatorType::StackAllocator; };
class scratch_allocator_enum_val_t { static constexpr AllocatorType allocator_type = AllocatorType::ScratchAllocator; };
class region_allocator_enum_val_t { static constexpr AllocatorType allocator_type = AllocatorType::RegionAllocator; };
// clang-format on

template <typename Allocator> struct enum_value_for_type
{
    static constexpr AllocatorType value = std::conditional_t<
        std::is_same_v<Allocator, heap_allocator_t>, heap_allocator_enum_val_t,
        std::conditional_t<
            std::is_same_v<Allocator, c_allocator_t>, c_allocator_enum_val_t,
            std::conditional_t<
                std::is_same_v<Allocator, block_allocator_t>,
                block_allocator_enum_val_t,
                std::conditional_t<
                    std::is_same_v<Allocator,
                                   segmented_array_block_allocator_t>,
                    segmented_array_block_allocator_enum_val_t,
                    std::conditional_t<
                        std::is_same_v<Allocator, stack_allocator_t>,
                        stack_allocator_enum_val_t,
                        std::conditional_t<
                            std::is_same_v<Allocator, scratch_allocator_t>,
                            scratch_allocator_enum_val_t,
                            std::conditional_t<
                                std::is_same_v<Allocator, region_allocator_t>,
                                region_allocator_enum_val_t,
                                std::false_type>>>>>>>::allocator_type;
};

// which interfaces each allocator has
constexpr uint8_t interface_bits[uint8_t(AllocatorType::MAX_ALLOCATOR_TYPE)] = {
    0b11111, // heap
    0b11111, // c allocator
    0b11111, // block allocator
    0b11111, // segmented array block allocator
    0b10011, // stack
    0b10111, // scratch
    0b11111, // region allocator
};

// clang-format off
constexpr uint8_t mask_alloc =          0b10000;
constexpr uint8_t mask_realloc =        0b01000;
constexpr uint8_t mask_free =           0b00100;
constexpr uint8_t mask_stack_realloc =  0b00010;
constexpr uint8_t mask_stack_free =     0b00001;
// clang-format on

inline constexpr bool has_alloc(AllocatorType type)
{
    if (uint8_t(type) >= uint8_t(AllocatorType::MAX_ALLOCATOR_TYPE))
        return false;
    return (interface_bits[uint8_t(type)] & mask_alloc) > 0;
}

inline constexpr bool has_realloc(AllocatorType type)
{
    if (uint8_t(type) >= uint8_t(AllocatorType::MAX_ALLOCATOR_TYPE))
        return false;
    return (interface_bits[uint8_t(type)] & mask_realloc) > 0;
}

inline constexpr bool has_free(AllocatorType type)
{
    if (uint8_t(type) >= uint8_t(AllocatorType::MAX_ALLOCATOR_TYPE))
        return false;
    return (interface_bits[uint8_t(type)] & mask_free) > 0;
}

inline constexpr bool has_stack_realloc(AllocatorType type)
{
    if (uint8_t(type) >= uint8_t(AllocatorType::MAX_ALLOCATOR_TYPE))
        return false;
    return (interface_bits[uint8_t(type)] & mask_stack_realloc) > 0;
}

inline constexpr bool has_stack_free(AllocatorType type)
{
    if (uint8_t(type) >= uint8_t(AllocatorType::MAX_ALLOCATOR_TYPE))
        return false;
    return (interface_bits[uint8_t(type)] & mask_stack_free) > 0;
}

// all allocators inherit from this
class dynamic_allocator_base_t
{
  protected:
    AllocatorType type;
};

class i_stack_free;                         // 0b00001 1
class i_stack_realloc;                      // 0b00010 2
class i_stack_realloc_i_stack_free;         // 0b00011 3
class i_free;                               // 0b00101 5
class i_stack_realloc_i_free;               // 0b00111 7
class i_realloc;                            // 0b01010 10
class i_realloc_i_stack_free;               // 0b01011 11
class i_realloc_i_free;                     // 0b01111 15
class i_alloc;                              // 0b10000 16
class i_alloc_i_stack_free;                 // 0b10001 17
class i_alloc_i_stack_realloc;              // 0b10010 18
class i_alloc_i_stack_realloc_i_stack_free; // 0b10011 19
class i_alloc_i_free;                       // 0b10101 21
class i_alloc_i_stack_realloc_i_free;       // 0b10111 23
class i_alloc_i_realloc;                    // 0b11010 26
class i_alloc_i_realloc_i_stack_free;       // 0b11011 27
class i_alloc_i_realloc_i_free;             // 0b11111 31

class i_stack_free : public stack_freer_t
{
  public:
    static constexpr uint8_t interfaces = 0b00001;
};

class i_stack_realloc : public stack_reallocator_t
{
  public:
    static constexpr uint8_t interfaces = 0b00010;
};

class i_stack_realloc_i_stack_free : public stack_reallocator_t,
                                     public stack_freer_t
{
  public:
    static constexpr uint8_t interfaces = 0b00011;
};

class i_free : public freer_t
{
  public:
    static constexpr uint8_t interfaces = 0b00101;
};

class is_stack_realloc_i_free : public stack_reallocator_t, public freer_t
{
  public:
    static constexpr uint8_t interfaces = 0b00111;
};

class i_realloc : public reallocator_t
{
  public:
    static constexpr uint8_t interfaces = 0b01010;
};

class i_realloc_i_stack_free : public reallocator_t, public stack_freer_t
{
  public:
    static constexpr uint8_t interfaces = 0b01011;
};

class i_realloc_i_free : public reallocator_t, public freer_t
{
  public:
    static constexpr uint8_t interfaces = 0b01111;
};

class i_alloc : public allocator_t
{
  public:
    static constexpr uint8_t interfaces = 0b10000;
};

class i_alloc_i_stack_free : public allocator_t, public stack_freer_t
{
  public:
    static constexpr uint8_t interfaces = 0b10001;
};

class i_alloc_i_stack_realloc : public allocator_t, public stack_reallocator_t
{
  public:
    static constexpr uint8_t interfaces = 0b10010;
};

class i_alloc_i_stack_realloc_i_stack_free : public allocator_t,
                                             public stack_reallocator_t,
                                             public stack_freer_t
{
  public:
    static constexpr uint8_t interfaces = 0b10011;
};

class i_alloc_i_free : public allocator_t, public freer_t
{
  public:
    static constexpr uint8_t interfaces = 0b10101;
};

class i_alloc_i_stack_realloc_i_free : public allocator_t,
                                       public stack_reallocator_t,
                                       public freer_t
{
  public:
    static constexpr uint8_t interfaces = 0b10101;
};

class i_alloc_i_realloc : public allocator_t, public reallocator_t
{
  public:
    static constexpr uint8_t interfaces = 0b10111;
};

class i_alloc_i_realloc_i_stack_free : public allocator_t,
                                       public reallocator_t,
                                       public stack_freer_t
{
  public:
    static constexpr uint8_t interfaces = 0b10111;
};

class i_alloc_i_realloc_i_free : public allocator_t,
                                 public reallocator_t,
                                 public freer_t
{
  public:
    static constexpr uint8_t interfaces = 0b11111;
};

} // namespace detail

template <typename Allocator, typename Interface>
inline constexpr Interface &
upcast(std::enable_if_t<
       std::is_base_of_v<detail::dynamic_allocator_base_t, Allocator> &&
           std::is_base_of_v<detail::allocator_interface_t, Interface> &&
           ((Interface::interfaces &
             detail::interface_bits[uint8_t(
                 detail::enum_value_for_type<Allocator>::value)]) ==
            Interface::interfaces),
       Allocator> &allocator) noexcept
{
    return *reinterpret_cast<Interface *>(&allocator);
}

} // namespace allo
