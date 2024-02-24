#pragma once
#include "ziglike/opt.h"
#include "ziglike/res.h"
#include "ziglike/slice.h"
#include "ziglike/status.h"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <type_traits>

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
    // if the allocation is too big (for example trying to allocate something
    // bigger than a block in a block allocator) but the allocator does have
    // more memory for further allocations. basically, this means "OOM, but try
    // something else, that might work". Halfway between InvalidArgument and OOM
    AllocationTooBig,

    // you requested a greater alignment than the allocator can provide.
    // guaranteed to not be produced if the allocators' properties meet
    // requirements
    AllocationTooAligned,
    // memory passed in to an allocator function could not concievably be owned
    // by that allocator, either by being outside its bounds or misaligned
    MemoryInvalid,
    // when using type checking, this indicates that you tried to free as a
    // different type that what was originally allocated
    InvalidType,
};

using allocation_status_t = zl::status<AllocationStatusCode>;

/// May either be a successfull allocation, or a status code failure. Check by
/// calling okay(), and if okay, call release() to get the allocated memory.
using allocation_result_t = zl::res<zl::slice<uint8_t>, AllocationStatusCode>;

struct allocator_requirements_t
{
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
  public:
    allocator_properties_t() = delete;
    friend class detail::memory_info_provider_t;
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

  private:
    // zero means theoretically limitless contiguous allocation is possible
    size_t m_maximum_contiguous_bytes;
    uint8_t m_maximum_alignment;

    inline constexpr allocator_properties_t(size_t max_contiguous_bytes,
                                            uint8_t max_alignment)
        : m_maximum_contiguous_bytes(max_contiguous_bytes),
          m_maximum_alignment(max_alignment)
    {
    }
};

using destruction_callback_t = void (*)(void *user_data);

class heap_allocator_t;
class c_allocator_t;
class stack_allocator_t;
class segmented_array_block_allocator_t;
class block_allocator_t;
class scratch_allocator_t;
class region_allocator_t;
class oneshot_allocator_t;

namespace detail {

class memory_info_provider_t
{
  public:
    [[nodiscard]] static const allocator_properties_t &
    _properties(const void *self) noexcept;

  protected:
    [[nodiscard]] static inline constexpr allocator_properties_t
    make_properties(size_t max_contiguous_bytes, uint8_t max_alignment) noexcept
    {
        return allocator_properties_t{max_contiguous_bytes, max_alignment};
    }

    [[nodiscard]] static inline constexpr size_t
    get_max_contiguous_bytes(const allocator_properties_t &properties) noexcept
    {
        return properties.m_maximum_contiguous_bytes;
    }

    [[nodiscard]] static inline constexpr uint8_t
    get_max_alignment(const allocator_properties_t &properties) noexcept
    {
        return properties.m_maximum_alignment;
    }
};

class allocator_interface_t
{};

class threadsafe_allocator_base_t
{};

class allocator_t : public memory_info_provider_t, public allocator_interface_t
{
  public:
    /// Request an allocation for some number of bytes with some alignment, and
    /// providing the typehash. If a non-typed allocator, 0 can be supplied as
    /// the hash.
    [[nodiscard]] static allocation_result_t
    _alloc_bytes(void *self, size_t bytes, uint8_t alignment_exponent,
                 size_t typehash) noexcept;
};

class stack_reallocator_t : public memory_info_provider_t,
                            public allocator_interface_t
{
  public:
    [[nodiscard]] static allocation_result_t
    _realloc_bytes(void *self, zl::slice<uint8_t> mem, size_t old_typehash,
                   size_t new_size, size_t new_typehash) noexcept;
};

class reallocator_t : public stack_reallocator_t
{};

class stack_freer_t : public allocator_interface_t
{
  public:
    static allocation_status_t _free_bytes(void *self, zl::slice<uint8_t> mem,
                                           size_t typehash) noexcept;
    /// Returns Okay if the free of the given memory would succeed, otherwise
    /// returns the error that would be returned if you tried to free.
    [[nodiscard]] static allocation_status_t
    _free_status(const void *self, zl::slice<uint8_t> mem,
                 size_t typehash) noexcept;
};

class freer_t : public stack_freer_t
{};

class destruction_callback_provider_t
{
  public:
    static allocation_status_t
    _register_destruction_callback(void *self, destruction_callback_t callback,
                                   void *user_data) noexcept;
};

/// Take a given number divisible by two and find what n is in 2^n = number.
/// Returns 64 (ie. 2^64) as an error value
inline constexpr uint8_t alignment_exponent(size_t alignment)
{
    constexpr auto bits = sizeof(size_t) * 8;
    for (size_t i = 0; i < bits; ++i) {
        if (((size_t(1) << i) & alignment) == alignment) {
            return i;
        }
    }
    return bits;
}

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
    ThreadsafeAllocator,
    OneshotAllocator,
    MAX_ALLOCATOR_TYPE
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
    0b11111, // threadsafe allocator
    0b01111, // oneshot allocator
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

template <typename Interface> inline constexpr bool interface_has_alloc()
{
    return (Interface::interfaces & mask_alloc) > 0;
}

template <typename Interface> inline constexpr bool interface_has_realloc()
{
    return (Interface::interfaces & mask_realloc) > 0;
}

template <typename Interface> inline constexpr bool interface_has_free()
{
    return (Interface::interfaces & mask_free) > 0;
}

template <typename Interface>
inline constexpr bool interface_has_stack_realloc()
{
    return (Interface::interfaces & mask_stack_realloc) > 0;
}

template <typename Interface> inline constexpr bool interface_has_stack_free()
{
    return (Interface::interfaces & mask_stack_free) > 0;
}

// all allocators inherit from this
class dynamic_allocator_base_t
{
  public:
    AllocatorType type;
};

// all threadsafe allocators inherit from this
class threadsafe_dynamic_allocator_base_t : public dynamic_allocator_base_t
{};

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

class i_stack_realloc : public stack_reallocator_t,
                        public destruction_callback_provider_t
{
  public:
    static constexpr uint8_t interfaces = 0b00010;
};

class i_stack_realloc_i_stack_free : public stack_reallocator_t,
                                     public stack_freer_t,
                                     public destruction_callback_provider_t
{
  public:
    static constexpr uint8_t interfaces = 0b00011;
};

class i_free : public freer_t
{
  public:
    static constexpr uint8_t interfaces = 0b00101;
};

class i_stack_realloc_i_free : public stack_reallocator_t,
                               public freer_t,
                               public destruction_callback_provider_t
{
  public:
    static constexpr uint8_t interfaces = 0b00111;
};

class i_realloc : public reallocator_t, public destruction_callback_provider_t
{
  public:
    static constexpr uint8_t interfaces = 0b01010;
};

class i_realloc_i_stack_free : public reallocator_t,
                               public stack_freer_t,
                               public destruction_callback_provider_t
{
  public:
    static constexpr uint8_t interfaces = 0b01011;
};

class i_realloc_i_free : public reallocator_t,
                         public freer_t,
                         public destruction_callback_provider_t
{
  public:
    static constexpr uint8_t interfaces = 0b01111;
};

class i_alloc : public allocator_t, public destruction_callback_provider_t
{
  public:
    static constexpr uint8_t interfaces = 0b10000;
};

class i_alloc_i_stack_free : public allocator_t,
                             public stack_freer_t,
                             public destruction_callback_provider_t
{
  public:
    static constexpr uint8_t interfaces = 0b10001;
};

class i_alloc_i_stack_realloc : public allocator_t,
                                public stack_reallocator_t,
                                public destruction_callback_provider_t
{
  public:
    static constexpr uint8_t interfaces = 0b10010;
};

class i_alloc_i_stack_realloc_i_stack_free
    : public allocator_t,
      public stack_reallocator_t,
      public stack_freer_t,
      public destruction_callback_provider_t
{
  public:
    static constexpr uint8_t interfaces = 0b10011;
};

class i_alloc_i_free : public allocator_t,
                       public freer_t,
                       public destruction_callback_provider_t
{
  public:
    static constexpr uint8_t interfaces = 0b10101;
};

class i_alloc_i_stack_realloc_i_free : public allocator_t,
                                       public stack_reallocator_t,
                                       public freer_t,
                                       public destruction_callback_provider_t
{
  public:
    static constexpr uint8_t interfaces = 0b10111;
};

class i_alloc_i_realloc : public allocator_t,
                          public reallocator_t,
                          public destruction_callback_provider_t
{
  public:
    static constexpr uint8_t interfaces = 0b11010;
};

class i_alloc_i_realloc_i_stack_free : public allocator_t,
                                       public reallocator_t,
                                       public stack_freer_t,
                                       public destruction_callback_provider_t
{
  public:
    static constexpr uint8_t interfaces = 0b11011;
};

class i_alloc_i_realloc_i_free : public allocator_t,
                                 public reallocator_t,
                                 public freer_t,
                                 public destruction_callback_provider_t
{
  public:
    static constexpr uint8_t interfaces = 0b11111;
};

template <uint8_t bits, typename Interface>
inline constexpr bool matches = Interface::interfaces == bits;

template <uint8_t bits, typename Interfaces, typename alternative>
using matches_or =
    std::conditional_t<matches<bits, Interfaces>, Interfaces, alternative>;

template <uint8_t bits> class invalid_interfaces
{
    static_assert(bits < 0,
                  "The provided allocator interfaces are not compatible.");
};

template <uint8_t bits>
using type_with_bits = matches_or<
    bits, i_alloc_i_realloc_i_free,
    matches_or<
        bits, i_alloc_i_realloc_i_stack_free,
        matches_or<
            bits, i_alloc_i_realloc,
            matches_or<
                bits, i_alloc_i_stack_realloc_i_free,
                matches_or<
                    bits, i_alloc_i_free,
                    matches_or<
                        bits, i_alloc_i_stack_realloc_i_stack_free,
                        matches_or<
                            bits, i_alloc_i_stack_realloc,
                            matches_or<
                                bits, i_alloc_i_stack_free,
                                matches_or<
                                    bits, i_alloc,
                                    matches_or<
                                        bits, i_realloc_i_free,
                                        matches_or<
                                            bits, i_realloc_i_stack_free,
                                            matches_or<
                                                bits, i_realloc,
                                                matches_or<
                                                    bits,
                                                    i_stack_realloc_i_free,
                                                    matches_or<
                                                        bits, i_free,
                                                        matches_or<
                                                            bits,
                                                            i_stack_realloc_i_stack_free,
                                                            matches_or<
                                                                bits,
                                                                i_stack_realloc,
                                                                matches_or<
                                                                    bits,
                                                                    i_stack_free,
                                                                    invalid_interfaces<
                                                                        bits>>>>>>>>>>>>>>>>>>;

template <typename... Ts> inline constexpr uint8_t get_bits_for_types()
{
    uint8_t bits = 0;
    ((bits |= Ts::interfaces), ...);
    return bits;
}

template <uint8_t index> inline constexpr uint8_t get_bits_for_index() noexcept
{
    static_assert(index < sizeof(detail::interface_bits));
    return detail::interface_bits[index];
}

template <typename Interface, typename Allocator>
constexpr bool can_upcast_to =
    (Interface::interfaces &
     get_bits_for_index<uint8_t(Allocator::enum_value)>()) ==
    Interface::interfaces;
} // namespace detail

template <typename Interface, typename InterfaceOrAllocator>
inline constexpr Interface &upcast(InterfaceOrAllocator &allocator) noexcept
{
    constexpr bool is_valid_interface =
        std::is_base_of_v<detail::allocator_interface_t, InterfaceOrAllocator>;
    constexpr bool is_valid_allocator =
        std::is_base_of_v<detail::dynamic_allocator_base_t,
                          InterfaceOrAllocator>;
    static_assert(is_valid_interface || is_valid_allocator,
                  "Invalid type trying to be upcasted: neither allocator "
                  "interface nor allocator.");

    static_assert(std::is_base_of_v<detail::allocator_interface_t, Interface>,
                  "Type trying to be upcasted to (the interface) is not an "
                  "allocator interface");

    if constexpr (is_valid_allocator) {
        static_assert(
            detail::can_upcast_to<Interface, InterfaceOrAllocator>,
            "The given allocator type does not implement the required "
            "interfaces to perform this upcast");
        return *reinterpret_cast<Interface *>(&allocator);
    } else {
        static_assert((InterfaceOrAllocator::interfaces &
                       Interface::interfaces) == Interface::interfaces,
                      "The type being upcasted does not implement all the "
                      "necessary interfaces to be cast to the target type.");
        return *reinterpret_cast<Interface *>(&allocator);
    }
}

using IAlloc = detail::i_alloc;
using IStackRealloc = detail::i_stack_realloc;
using IStackFree = detail::i_stack_free;
using IRealloc = detail::i_realloc;
using IFree = detail::i_free;

template <typename... Interfaces>
using allocator_with =
    detail::type_with_bits<detail::get_bits_for_types<Interfaces...>()>;

template <typename InterfaceOrAllocator> class threadsafe_t
{
    static constexpr bool is_valid_interface =
        std::is_base_of_v<InterfaceOrAllocator,
                          detail::allocator_interface_t> &&
        detail::interface_has_alloc<InterfaceOrAllocator>() &&
        detail::interface_has_stack_free<InterfaceOrAllocator>() &&
        detail::interface_has_stack_realloc<InterfaceOrAllocator>();
    static constexpr bool is_valid_allocator =
        std::is_base_of_v<InterfaceOrAllocator,
                          detail::dynamic_allocator_base_t> &&
        detail::can_upcast_to<allocator_with<IAlloc, IStackFree, IStackRealloc>,
                              InterfaceOrAllocator>;
    static_assert(
        !(is_valid_interface && is_valid_allocator),
        "ParentAllocator type is both a valid interface and allocator. Is this "
        "a custom allocator with incorrect inheritance?");
    static_assert(is_valid_interface || is_valid_allocator,
                  "Invalid type provided for parent allocator");

    static constexpr bool has_free()
    {
        if constexpr (is_valid_interface) {
            return detail::interface_has_free<InterfaceOrAllocator>();
        } else {
            return detail::has_free(InterfaceOrAllocator::enum_value);
        }
    }

    static constexpr bool has_realloc()
    {
        if constexpr (is_valid_interface) {
            return detail::interface_has_realloc<InterfaceOrAllocator>();
        } else {
            return detail::has_realloc(InterfaceOrAllocator::enum_value);
        }
    }

    static constexpr bool has_alloc()
    {
        if constexpr (is_valid_interface) {
            return detail::interface_has_alloc<InterfaceOrAllocator>();
        } else {
            return detail::has_alloc(InterfaceOrAllocator::enum_value);
        }
    }

    template <typename ThisType = InterfaceOrAllocator>
    [[nodiscard]] allocation_result_t alloc_bytes(
        std::enable_if_t<has_alloc() &&
                             std::is_same_v<InterfaceOrAllocator, ThisType>,
                         size_t>
            bytes,
        uint8_t alignment_exponent, size_t typehash) noexcept
    {
        return m_parent.alloc_bytes(bytes, alignment_exponent, typehash);
    }

    template <typename ThisType = InterfaceOrAllocator>
    [[nodiscard]] allocation_result_t realloc_bytes(
        std::enable_if_t<has_realloc() &&
                             std::is_same_v<InterfaceOrAllocator, ThisType>,
                         zl::slice<uint8_t>>
            mem,
        size_t old_typehash, size_t new_size, size_t new_typehash) noexcept
    {
        return m_parent.realloc_bytes(mem, old_typehash, new_size,
                                      new_typehash);
    }

    template <typename ThisType = InterfaceOrAllocator>
    allocation_status_t free_bytes(
        std::enable_if_t<std::is_same_v<ThisType, InterfaceOrAllocator> &&
                             has_free(),
                         zl::slice<uint8_t>>
            mem,
        size_t typehash)
    {
        return m_parent.free_bytes(mem, typehash);
    }

    template <typename T = allocator_properties_t>
    [[nodiscard]] inline constexpr const std::enable_if_t<
        std::is_same_v<T, allocator_properties_t> &&
            std::is_base_of_v<detail::memory_info_provider_t,
                              InterfaceOrAllocator>,
        T> &
    properties() const
    {
        return m_parent.properties();
    }

    template <typename T = destruction_callback_t>
    allocation_status_t register_destruction_callback(
        std::enable_if_t<
            std::is_base_of_v<detail::destruction_callback_provider_t,
                              InterfaceOrAllocator>,
            T>
            callback,
        void *user_data) noexcept;

  private:
    InterfaceOrAllocator &m_parent;
};
} // namespace allo
#ifdef ALLO_HEADER_ONLY
#include "allo/impl/allocator_interfaces.h"
#endif
