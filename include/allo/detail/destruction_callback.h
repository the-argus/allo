#pragma once
#include "allo/detail/alignment.h"
#include "allo/status.h"
#include <cassert>
#include <cstddef>

namespace allo {
using destruction_callback_t = void (*)(void* user_data);
namespace detail {
struct destruction_callback_entry_t
{
    destruction_callback_t callback;
    void* user_data;
};

struct destruction_callback_entry_list_node_t
{
    destruction_callback_entry_list_node_t* prev;
    destruction_callback_entry_t entries[]; // NOLINT
};

inline constexpr std::size_t calculate_bytes_needed_for_destruction_callback(
    std::size_t num_entries) noexcept
{
    return sizeof(destruction_callback_entry_list_node_t) +
           (num_entries * sizeof(destruction_callback_entry_t));
}

template <std::size_t num_entries> struct bytes_needed_for_destruction_callback
{
    static constexpr std::size_t value =
        calculate_bytes_needed_for_destruction_callback(num_entries);
};

template <std::size_t num_entries>
inline constexpr std::size_t bytes_needed_for_destruction_callback_v =
    bytes_needed_for_destruction_callback<num_entries>::value;

inline constexpr void call_all_destruction_callback_arrays(
    destruction_callback_entry_list_node_t* end_node, size_t items_per_entry,
    size_t items_in_end) noexcept
{
    assert(items_in_end <= items_per_entry);
    auto* iter = end_node;
    // handle first iteration
    {
        if (!iter)
            return;
        for (std::size_t i = 0; i < items_in_end; ++i) {
            destruction_callback_entry_t& entry = iter->entries[i];
            entry.callback(entry.user_data);
        }
        iter = iter->prev;
    }

    // all other list nodes are considered to be full of entries
    while (iter) {
        for (std::size_t i = 0; i < items_per_entry; ++i) {
            destruction_callback_entry_t& entry = iter->entries[i];
            entry.callback(entry.user_data);
        }
        iter = iter->prev;
    }
}

inline constexpr void call_all_destruction_callbacks(
    destruction_callback_entry_list_node_t* end_node) noexcept
{
    auto* iter = end_node;
    while (iter) {
        destruction_callback_entry_t& entry = *iter->entries;
        entry.callback(entry.user_data);
        iter = iter->prev;
    }
}

template <typename T>
inline constexpr allocation_status_t
register_destruction_callback_with_only_one_item_per_array(
    T* thisptr, destruction_callback_entry_list_node_t*& headptr,
    destruction_callback_t callback, void* user_data)
{
    if (!callback) {
        assert(callback != nullptr);
        return AllocationStatusCode::InvalidArgument;
    }
    auto res =
        thisptr->alloc_bytes(detail::bytes_needed_for_destruction_callback_v<1>,
                             detail::nearest_alignment_exponent(
                                 alignof(destruction_callback_entry_t)),
                             0);
    if (!res.okay())
        return res.err();
    auto& newentry = *reinterpret_cast<destruction_callback_entry_list_node_t*>(
        res.release_ref().data());
    // initialize the new destruction callback entry
    newentry.prev = headptr;
    newentry.entries[0] = detail::destruction_callback_entry_t{
        .callback = callback,
        .user_data = user_data,
    };
    // it is new head of list
    headptr = &newentry;
    return AllocationStatusCode::Okay;
}

} // namespace detail
} // namespace allo
