#pragma once
#include "ziglike/slice.h"
#ifndef ALLO_DISABLE_TYPEINFO
#ifndef ALLO_USE_RTTI
#include "ctti/typename.hpp"
#endif
#else
#error "ALLO_DISABLE_TYPEINFO not fully implemented"
#endif
#include <cstdint>
#include <type_traits>

#ifndef ALLO_NOEXCEPT
#define ALLO_NOEXCEPT noexcept
#endif
namespace allo {

/// A very simple allocator which takes in a fixed buffer of memory and
/// allocates randomly sized items within that buffer. They can only be freed in
/// the opposite order that they were allocated.
class stack_allocator_t
{
  public:
    // cannot be default constructed or copied
    stack_allocator_t() = delete;
    stack_allocator_t(const stack_allocator_t &) = delete;
    stack_allocator_t &operator=(const stack_allocator_t &) = delete;

    // can be explicitly constructed from a buffer of existing memory.
    // it will modify this memory, but not free it on destruction.
    explicit stack_allocator_t(zl::slice<uint8_t> memory) ALLO_NOEXCEPT;

    // can be moved
    stack_allocator_t(stack_allocator_t &&) ALLO_NOEXCEPT;
    stack_allocator_t &operator=(stack_allocator_t &&) ALLO_NOEXCEPT;

    // no need to do anything upon destruction since this is non-owning
    ~stack_allocator_t() = default;

    /// Get a pointer to a newly allocated and constructed object.
    /// Returns nullptr on OOM.
    template <typename T, typename... Args>
    [[nodiscard]] inline T *alloc(Args &&...args) ALLO_NOEXCEPT
    {
        static_assert(alignof(T) <= alignof(previous_state_t),
                      "Alignment of type passed in to stack_allocator_t::alloc "
                      "is larger than the bookkeeping data alignment and size, "
                      "which breaks memory layout guarantees.");
        static_assert(
            std::is_constructible_v<T, decltype(args)...>,
            "Attempt to alloc a type with forwarded constructor args, but no "
            "matching constructor for the type was found.");
        static_assert(std::is_destructible_v<T>,
                      "Type must be destructible to be allocated.");
        if (auto *spot = reinterpret_cast<T *>(
                inner_alloc(alignof(T), sizeof(T)))) [[likely]] {
            new (spot) T(std::forward<decltype(args)>(args)...);
            // store unique identifier for this type
#ifdef ALLO_USE_RTTI
            m_last_type = typeid(T).hash_code();
#else
            m_last_type = ctti::nameof<T>().hash();
#endif
            return spot;
        } else {
            return nullptr;
        }
    }

    /// Free a pointer allocated with this allocator. It is undefined behavior
    /// for the pointer to be a different type than the one it was allocated as.
    template <typename T> [[nodiscard]] inline bool free(T *item) ALLO_NOEXCEPT
    {
        static_assert(std::is_destructible_v<T>,
                      "Attempt to free type which is not nothrow destructible. "
                      "Was this pointer even allocated with this allocator?");
        if (
#ifdef ALLO_USE_RTTI
            typeid(T).hash_code()
#else
            ctti::nameof<T>().hash()
#endif

            != m_last_type) [[unlikely]] {
            return false;
        } else if (void *freed = inner_free(alignof(T), sizeof(T), item))
            [[likely]] {
            reinterpret_cast<T *>(freed)->~T();
            return true;
        }
        return false;
    }

    /// Zero all the memory in this stack allocator's buffer
    void zero() ALLO_NOEXCEPT;

    friend class stack_allocator_dynamic_t;

  private:
    /// the information placed underneath every allocation in the stack
    struct previous_state_t
    {
        size_t memory_available;
        size_t type_hashcode;
    };

    void *inner_alloc(size_t align, size_t typesize) ALLO_NOEXCEPT;
    void *raw_alloc(size_t align, size_t typesize) ALLO_NOEXCEPT;
    // inner_free returns a pointer to the space that was just freed, or nullptr
    // on failure
    void *inner_free(size_t align, size_t typesize, void *item) ALLO_NOEXCEPT;

    zl::slice<uint8_t> m_memory;
    size_t m_first_available = 0;
    size_t m_last_type = 0;
};
} // namespace allo

#ifdef ALLO_HEADER_ONLY
#include "allo/impl/stack_allocator.h"
#endif
