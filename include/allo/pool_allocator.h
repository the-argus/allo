#pragma once
#include "allo/allocator_interfaces.hpp"
#include "thelib/result.hpp"
#include "thelib/slice.hpp"
#include <array>

namespace allo {

struct pool_allocator_generational_options_t
{
    interfaces::random_allocator_t allocator;
    interfaces::AllocationType allocation_type;
    /// Whether the pool allocator will try to call its allocator's realloc
    /// function when it runs out of space.
    bool reallocating = false;
    /// If reallocating, this determines by what factor the allocator will try
    /// to increase its size
    float reallocation_ratio = 1.5f;
};

// TODO: make this type thread safe

/// Allocates many instances of a single type. You can use it to get an
/// allocated instance of a type (the position in the container is not
/// guaranteed) or to iterate over all instance of the type allocated with this
/// allocator.
///
/// Can verify whether a handle points to a valid item and whether you have
/// double freed.
/// NOT thread safe.
///
/// If reallocating, the allocator will be able to perform more allocations than
/// were initially reserved in the constructor. If not reallocating, the pool
/// will return a failure status code every time you try to allocate a new
/// object with no free space.
///
/// If generational, handles and allocated entries of the ojects will include a
/// generation, allowing for verifying whether handles to memory are valid.
template <typename T, pool_allocator_generational_options_t options,
          typename index_t = size_t, typename gen_t = size_t>
requires(
#ifdef ALLO_POOL_ALLOCATOR_T_NO_NOTHROW
    std::is_destructible_v<T>
#else
    std::is_nothrow_destructible_v<T>
#endif
    ) class pool_allocator_generational_t
{
  public:
    using type = T;
    inline static constexpr pool_allocator_generational_options_t
        passed_options = options;
    static_assert(
        passed_options.reallocation_ratio > 1,
        "Refusing to create allocator with reallocation ratio of 1 or less.");

  private:
    union payload_t
    {
        T data;
        index_t index;
        // constructor and destructor which leave memory uninitialized because
        // we do that manually
        inline constexpr ~payload_t() TESTING_NOEXCEPT {}
        inline constexpr payload_t() TESTING_NOEXCEPT {}
    };

#ifdef ALLO_LOGGING
    static constexpr bool logging = true;
#else
    static constexpr bool logging = false;
#endif

    /// Performs the initial allocation for a buffer
    template <typename U>
    static inline lib::slice_t<U>
    init_buffer(size_t reserved_spots) TESTING_NOEXCEPT
    {
        auto res = passed_options.allocator.alloc(
            passed_options.allocation_type, sizeof(U), reserved_spots);
        if (!res.okay()) [[unlikely]] {
            if constexpr (logging) {
                LN_ERROR("Initial reservation allocation for pool allocator "
                         "failed.");
            }
            ABORT();
        }
        auto memslice = res.release();
        assert(memslice.size() == sizeof(U) * reserved_spots);
        return lib::raw_slice(*reinterpret_cast<U *>(memslice.data()),
                              reserved_spots);
    }

  public:
    struct handle_t
    {
        friend class pool_allocator_generational_t;

        /// Get the index of this handle. Is only intended to be used in special
        /// cases, if youre using this and you don't know what you're doing,
        /// please stop.
        [[nodiscard]] constexpr inline index_t index() const noexcept
        {
            return m_index;
        }
        /// Get the generation of this handle. Is only intended to be used in
        /// special cases, if youre using this and you don't know what you're
        /// doing, please stop.
        [[nodiscard]] constexpr inline gen_t generation() const noexcept
        {
            return m_generation;
        }

        inline constexpr friend bool
        operator==(const handle_t &a, const handle_t &b) TESTING_NOEXCEPT
        {
            return a.m_index == b.m_index && b.m_generation == a.m_generation;
        };

      private:
        inline constexpr handle_t(index_t _index, gen_t _generation) noexcept
            : m_index(_index), m_generation(_generation)
        {
        }
        index_t m_index;
        gen_t m_generation;
    };

    enum class alloc_err_code_e : uint8_t
    {
        Okay,
        ResultReleased,
        OOM,
    };

    enum class get_handle_err_code_e : uint8_t
    {
        Okay,
        ResultReleased,
        Null,               // the pointer passed is null
        ItemNoLongerValid,  // the item at the pointer is no longer valid
        ItemNotInAllocator, // the item being pointed at is invalid because it
                            // is outside the currently allocated space this
                            // allocator is using
        AllocationShrunk,   // item is within allocation but off the end. very
                            // similar to ItemNoLongerValid
    };

    enum class lookup_return_code_e : uint8_t
    {
        Okay = 0, // gonna be comparing against this a lot
        ResultReleased,
        IndexOutOfRange,
        // when the index is in range but the thing is points to is
        // uninitialized
        InvalidIndex,
        OldGeneration,
        InvalidGeneration,
        // object is null, essentially
        // this is an error, even when returned from free() because it means
        // the thing trying to be freed was already so
        Freed,
    };

    inline explicit pool_allocator_generational_t(size_t reserved_spots)
        TESTING_NOEXCEPT
        : m_items_buffer(init_buffer<typename decltype(m_items_buffer)::type>(
              reserved_spots == 0 ? 1 : reserved_spots)),
          m_activity_buffer(
              init_buffer<typename decltype(m_activity_buffer)::type>(
                  reserved_spots == 0 ? 1 : reserved_spots)),
          m_generation_buffer(
              init_buffer<typename decltype(m_generation_buffer)::type>(
                  reserved_spots == 0 ? 1 : reserved_spots))
    {
        const size_t actual_reserved = reserved_spots == 0 ? 1 : reserved_spots;
        if constexpr (logging) {
            if (reserved_spots == 0) {
                // TODO: add a reserve() function to pool allocators to delay
                // the initial allocation of memory
                LN_WARN("Attempt to initialize a pool allocator with no open "
                        "spots. Increasing reservation to 1, to avoid any "
                        "attempt to allocate a buffer of 0 size.");
            }
            if (reserved_spots < 2) {
                if constexpr (passed_options.reallocating) {
                    LN_INFO("Initializing pool allocator with less than two "
                            "spots leads to suboptimal allocation. Consider "
                            "using a random allocator instead, to allocate "
                            "only one of an item.");
                }
            }
        }
        m_spots_free = actual_reserved;
        m_last_free_index = 0;

        // initialize all the spots to point to the next spot as empty
        size_t index = 0;
        for (payload_t &payload : m_items_buffer) {
            ++index;
            // 0 points at 1, etc
            payload.index = index;
        }
    }

    /// Allocate and construct a new item into the buffer, constructed using the
    /// arguments passed in
    [[nodiscard]] inline lib::result_t<handle_t, alloc_err_code_e>
    alloc_new(auto &&...args) TESTING_NOEXCEPT
    {
        // Try to reallocate if we are configured to do that
        if constexpr (passed_options.reallocating) {
            if (m_spots_free == 0) [[unlikely]] {
                this->realloc();
            }
        }

        // if we are out of memory or if reallocation failed, abort with OOM
        if (m_spots_free == 0) [[unlikely]] {
            return alloc_err_code_e::OOM;
        }

        index_t selected_index = m_last_free_index;
        payload_t &target = m_items_buffer.data()[m_last_free_index];
        assert(m_activity_buffer.data()[selected_index] == false);
        m_last_free_index = target.index;
        new (&target.data) T(std::forward<decltype(args)>(args)...);
        auto &gen = m_generation_buffer.data()[selected_index];
        m_activity_buffer.data()[selected_index] = true; // is now active
        ++gen; // increase generation now that its been modified
        if (gen > m_max_generation) {
            m_max_generation = gen;
        }
        --m_spots_free;

        if (selected_index >= m_end_guess) {
            m_end_guess = selected_index + 1;
        }

        return handle_t(selected_index, gen);
    }

    /// Reverse-engineer a handle from a pointer to an item.
    [[nodiscard]] inline lib::result_t<handle_t, get_handle_err_code_e>
    get_handle_from_item(const T *item) TESTING_NOEXCEPT
    {
        static_assert(alignof(T) ==
                      alignof(decltype(m_items_buffer.data()[0])));
        static_assert(sizeof(T) == sizeof(m_items_buffer.data()[0]));

        if (item) [[likely]] {
            if ((void *)item >= (void *)m_items_buffer.data() &&
                (void *)item <
                    (void *)(m_items_buffer.data() + m_items_buffer.size())) {

                if ((void *)item >=
                    (void *)(m_items_buffer.data() + m_end_guess)) {
                    return get_handle_err_code_e::AllocationShrunk;
                } else {
                    T *begin = &m_items_buffer.data()->data;
                    assert(((void *)begin) == ((void *)m_items_buffer.data()));
                    assert(begin <= item);
                    index_t index = item - begin;
                    assert(index < m_activity_buffer.size());
                    if (m_activity_buffer.data()[index]) {
                        return handle_t(index,
                                        m_generation_buffer.data()[index]);
                    } else {
                        return get_handle_err_code_e::ItemNoLongerValid;
                    }
                }
            }
            return get_handle_err_code_e::ItemNotInAllocator;
        } else {
            return get_handle_err_code_e::Null;
        }
    }

    /// Attempt to get an item in the allocator with a generational handle.
    /// Does bounds-checking, generation checking, and double free checking.
    inline constexpr lib::result_t<T &, lookup_return_code_e>
    get(const handle_t &handle) TESTING_NOEXCEPT
    {
        auto res = inner_lookup(handle);
        if (!res.okay()) [[unlikely]] {
            return res.status();
        }
        auto &ref = res.release().data;
        return const_cast<T &>(ref);
    }

    /// Attempt to get an item in the allocator with a generational handle.
    /// Does bounds-checking, generation checking, and double free checking.
    inline constexpr lib::result_t<const T &, lookup_return_code_e>
    get_const(const handle_t &handle) const TESTING_NOEXCEPT
    {
        auto res = inner_lookup(handle);
        if (!res.okay()) [[unlikely]] {
            return res.status();
        }
        return res.release().data;
    }

    inline lib::status_t<lookup_return_code_e>
    free(const handle_t &handle) TESTING_NOEXCEPT
    {
        auto response = inner_lookup(handle);

        // convert invalid lookups into a status_t
        if (!response.okay())
            return response.status();

        auto &itemref = const_cast<payload_t &>(response.release());

        // destroy, mark as free, increase generation
        itemref.data.~T();
        ++m_generation_buffer.data()[handle.index()];
        m_activity_buffer.data()[handle.index()] = false;

        // overwrite destroyed object with an index pointing to the last free
        // index
        itemref.index = m_last_free_index;

        // mark this location as the next available spot for allocation
        m_last_free_index = handle.index();

        ++m_spots_free;

        return lookup_return_code_e::Okay;
    }

    // the pool allocator owns its allocation
    inline ~pool_allocator_generational_t() TESTING_NOEXCEPT
    {
        for (int i = 0; i < m_end_guess; ++i) {
            if (m_activity_buffer.data()[i])
                m_items_buffer.data()[i].data.~T();
        }

        std::array<lib::status_t<interfaces::status_code_e>, 3> errcodes = {
            passed_options.allocator
                .free(passed_options.allocation_type, m_items_buffer.data(),
                      m_items_buffer.size() *
                          sizeof(typename decltype(m_items_buffer)::type))
                .status(),
            passed_options.allocator
                .free(passed_options.allocation_type, m_activity_buffer.data(),
                      m_activity_buffer.size() *
                          sizeof(typename decltype(m_activity_buffer)::type))
                .status(),
            passed_options.allocator
                .free(passed_options.allocation_type,
                      m_generation_buffer.data(),
                      m_generation_buffer.size() *
                          sizeof(typename decltype(m_generation_buffer)::type))
                .status(),
        };
        if constexpr (logging) {
            uint8_t index = 0;
            for (auto code : errcodes) {
                if (!code.okay()) [[unlikely]] {
                    LN_ERROR_FMT(
                        "Attempted to free pool allocator on destruction, "
                        "but got error code {} for buffer index {}",
                        fmt::underlying(code.status()), index);
                }
                ++index;
            }
        }
    }

    struct Iterator
    {
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = T;
        using pointer = T *;
        using reference = T &;

        inline constexpr Iterator(pool_allocator_generational_t &parent,
                                  index_t index) TESTING_NOEXCEPT
            : m_index(index),
              m_parent(parent)
        {
        }

        inline constexpr reference operator*() const TESTING_NOEXCEPT
        {
            auto res = m_parent.inner_lookup_index(m_index);
            if (!res.okay()) [[unlikely]] {
                if constexpr (logging) {
                    LN_FATAL("Attempt to access iterator which was pointing to "
                             "an invalid element.");
                }
                ABORT();
            }
            return res.release().data;
        }

        inline constexpr pointer operator->() TESTING_NOEXCEPT
        {
            return &(*this);
        }

        // Prefix increment
        inline constexpr Iterator &operator++() TESTING_NOEXCEPT
        {
            index_t end_guess = m_parent.m_end_guess;
            for (index_t i = m_index + 1; i < end_guess; ++i) {
                if (m_parent.m_activity_buffer.data()[i]) {
                    m_index = i;
                    return *this;
                }
            }

            // if we made it past the whole for loop then we reached the end
            // check if max_index_guess is the same to be certain that the
            // parent wasn't altered
            if (m_parent.m_activity_buffer.data()[m_index]) [[likely]] {
                // no one is after us, so the new estimated max index is our
                // index
                m_parent.m_end_guess = m_index + 1;
                // make ourselves the end()
                m_index = m_index + 1;
                return *this;
            } else [[unlikely]] {
                // our current iteration is invalid
                if constexpr (logging) {
                    LN_WARN("Called pool_allocator_iterable_t's iterator ++ "
                            "operator, but it is pointing to an invalid index. "
                            "Either the iterator was unsafely manipulated or "
                            "this is a programmer error in the iterator's "
                            "implementation.");
                }
                m_index = m_parent.m_end_guess;
                return *this;
            }
        }

        // Postfix increment
        // NOLINTNEXTLINE
        inline constexpr Iterator operator++(int) TESTING_NOEXCEPT
        {
            Iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        inline constexpr friend bool
        operator==(const Iterator &a, const Iterator &b) TESTING_NOEXCEPT
        {
            return a.m_index == b.m_index;
        };

        // TODO: implement this (ran into template deducible type problems)
        friend struct fmt::formatter<Iterator>;

      private:
        pool_allocator_generational_t &m_parent;
        index_t m_index;
    };

    inline constexpr Iterator begin() TESTING_NOEXCEPT
    {
        return Iterator(*this, 0);
    }
    inline constexpr Iterator end() TESTING_NOEXCEPT
    {
        return Iterator(*this, m_end_guess);
    }

    /// Return the number of items of type T that can fit in the current
    /// allocation.
    [[nodiscard]] inline constexpr size_t capacity() TESTING_NOEXCEPT
    {
        return m_items_buffer.size();
    }

    /// Return the number of items currently allocated within this allocator.
    [[nodiscard]] inline constexpr size_t size() TESTING_NOEXCEPT
    {
        return m_items_buffer.size() - m_spots_free;
    }

    /// Return the number of items that can be allocated in the allocator
    /// without performing reallocation and potential OOM.
    [[nodiscard]] inline constexpr size_t spots_available() TESTING_NOEXCEPT
    {
        return capacity() - size();
    }

  private:
    /// Return a mutable reference to an item pointed at by a handle, or an err
    /// code as to why the handle was invalid. used by both free() and get()
    inline constexpr lib::result_t<const payload_t &, lookup_return_code_e>
    inner_lookup(const handle_t &handle) const TESTING_NOEXCEPT
    {
        using code = lookup_return_code_e;

        if (handle.index() >= m_end_guess)
            return code::IndexOutOfRange;

        if (handle.generation() == 0 || handle.generation() > m_max_generation)
            return code::InvalidGeneration;

        if (!m_activity_buffer.data()[handle.index()])
            return code::Freed;

        {
            const auto gen = m_generation_buffer.data()[handle.index()];
            if (gen == 0)
                return code::InvalidIndex;

            if (gen < handle.generation())
                return code::InvalidGeneration;

            if (gen > handle.generation())
                return code::OldGeneration;
        }

        return m_items_buffer.data()[handle.index()];
    }

    /// Lookup based only on an index and no generation. Used by iterator
    inline constexpr lib::result_t<payload_t &, lookup_return_code_e>
    inner_lookup_index(index_t index) TESTING_NOEXCEPT
    {
        using code = lookup_return_code_e;

        if (index >= m_end_guess)
            return code::IndexOutOfRange;

        if (!m_activity_buffer.data()[index])
            return code::Freed;

        return m_items_buffer.data()[index];
    }

    /// Increases the size of the allocated buffer by the allocation ratio.
    /// Leaves new memory uninitialized. Returns the number of new items alloced
    template <typename U>
    inline lib::result_t<size_t, alloc_err_code_e>
    realloc_buffer(lib::slice_t<U> &buffer) TESTING_NOEXCEPT
        requires(passed_options.reallocating)
    {
        const size_t bufsize = buffer.size();
        const size_t bufsize_bytes = buffer.size() * sizeof(U);
        const size_t new_bufsize = std::ceil(static_cast<float>(bufsize) *
                                             passed_options.reallocation_ratio);
        const size_t new_bufsize_bytes = new_bufsize * sizeof(U);
        assert(new_bufsize > bufsize);

        auto res = passed_options.allocator.realloc(
            passed_options.allocation_type, buffer.data(), bufsize_bytes,
            new_bufsize_bytes);

        if (!res.okay()) [[unlikely]] {
            if constexpr (logging) {
                LN_FATAL("Reallocation of pool allocator failed, aborting "
                         "reallocation with OOM");
            }
            return alloc_err_code_e::OOM;
        }
        auto slice = res.release();
        assert(slice.size() == new_bufsize_bytes);
        buffer =
            lib::raw_slice(*reinterpret_cast<U *>(slice.data()), new_bufsize);
        return new_bufsize - bufsize;
    }

    inline lib::status_t<alloc_err_code_e> realloc() TESTING_NOEXCEPT
        requires(passed_options.reallocating)
    {
        assert(m_spots_free == 0);
        // increase size of buffers but leave new memory uninitialized
        const auto original_buffer_size = m_items_buffer.size();
        assert(m_items_buffer.size() == m_activity_buffer.size() &&
               m_activity_buffer.size() == m_generation_buffer.size());
        auto res_items = realloc_buffer(m_items_buffer);
        if (!res_items.okay()) [[unlikely]]
            return res_items.status();
        const size_t new_items = res_items.release();
        auto res_activity_bools = realloc_buffer(m_activity_buffer);
        if (!res_activity_bools.okay()) [[unlikely]]
            return res_activity_bools.status();
        const size_t new_activity_bools = res_activity_bools.release();
        auto res_generations = realloc_buffer(m_generation_buffer);
        if (!res_generations.okay()) [[unlikely]]
            return res_generations.status();
        const size_t new_generations = res_generations.release();
        assert(new_activity_bools == new_generations &&
               new_generations == new_items);
        m_spots_free += new_items;

        index_t index = original_buffer_size;
        for (payload_t &item : lib::slice_t<payload_t>(
                 m_items_buffer, original_buffer_size, m_items_buffer.size())) {
            // each item points at the next as the next free index
            ++index;
            item.index = index;
        }
        // set all new activity bools to false
        std::memset(m_activity_buffer.data() + original_buffer_size, false,
                    new_items);
        // set all generations to 0
        std::memset(m_generation_buffer.data() + original_buffer_size, 0,
                    new_items);
        m_last_free_index = original_buffer_size;
        return alloc_err_code_e::Okay;
    }

    lib::slice_t<payload_t> m_items_buffer;
    lib::slice_t<bool> m_activity_buffer;
    lib::slice_t<gen_t> m_generation_buffer;
    size_t m_spots_free;
    size_t m_last_free_index;
    /// The largest index currently allocated. Respected by iterators as the
    /// stopping point for iteration. Iterators may set it to be lower and
    /// allocating an item at a higher position will set it to be higher.
    size_t m_end_guess = 0;
    /// The largest recorded generation within this allocator
    size_t m_max_generation;
};

} // namespace allo
