#include "allo/detail/usage_profiling_callback.h"
#ifndef ALLO_PROFILE_MEMORY_USAGE_TESTING
#warning \
    "attempt to compile profiling without ALLO_PROFILE_MEMORY_USAGE_TESTING defined"
#else

struct Node
{
    allo::bytes_t data;
    std::vector<Node> children;
};

size_t test_num;
std::vector<Node> root_nodes;

void insert(allo::bytes_t item)
{
    std::vector<Node>* iterator = &root_nodes;
    while (1) {
        bool found_better_match = false;

        for (auto& node : *iterator) {
            if (zl::memcontains(node.data, item)) {
                iterator = &node.children;
                found_better_match = true;
                break;
            }
        }

        if (!found_better_match)
            break;
        // otherwise try again with the new iteration
    }
    iterator->push_back(Node{.data = item});
}

template <typename Callback>
void traverse_leaf_nodes(std::vector<Node>& nodes, Callback&& callback)
{
    for (auto& node : nodes) {
        if (node.children.empty()) {
            callback(node);
        } else {
            traverse_leaf_nodes(nodes,
                                std::forward<decltype(callback)>(callback));
        }
    }
}

namespace allo::detail {

void allocated(detail::abstract_allocator_t& self, any_allocator_t parent,
               bytes_t bytes_allocated)
{
    insert(bytes_allocated);
}

void begin_profile() { root_nodes = {}; }

void end_profile()
{
    // tally up total allocated
    size_t bytes = 0;
    for (auto& node : root_nodes) {
        bytes += node.data.size();
    }
    // tally up leaf nodes
    size_t bytes_used = 0;
    traverse_leaf_nodes(root_nodes, [&bytes_used](Node& leaf) {
        assert(leaf.children.empty());
        bytes_used += leaf.data.size();
    });

    printf("profile #%zu------------------------------------\n", test_num);
    printf("bytes allocated: %zu\n", bytes);
    printf("bytes used: %zu\n", bytes_used);
    ++test_num;
}

} // namespace allo::detail
#endif
