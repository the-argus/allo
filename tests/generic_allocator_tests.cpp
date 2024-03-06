#include "generic_allocator_tests.h"
#include "allo/typed_allocation.h"
#include "doctest.h"

struct CharNode;
struct LinkedString
{
    CharNode *first = nullptr;
    size_t length = 0;
    allo::DynamicAllocatorRef parent;
    void append(char b);
    LinkedString() = delete;
    LinkedString(allo::DynamicAllocatorRef _parent) : parent(_parent) {}
};

struct CharNode
{
    inline constexpr CharNode(char b) : contents(b), next(nullptr) {}
    CharNode *next;
    char contents;
};

void LinkedString::append(char b)
{
    auto mnewnode = allo::construct_one<CharNode>(parent, b);
    REQUIRE(mnewnode.okay());
    CharNode &newnode = mnewnode.release();
    CharNode *iter = first;
    if (iter) {
        while (iter->next != nullptr) {
            iter = iter->next;
        }
        iter->next = &newnode;
    } else {
        first = &newnode;
    }
    ++length;
}

namespace allo::tests {
void allocate_object_with_linked_list(DynamicAllocatorRef ally)
{
    constexpr std::array test_str{
        "hello",
        "what?",
        "the seventh son of the seventh son",
        "123456789",
    };

    for (const char *str : test_str) {
        auto mlinked = construct_one<LinkedString>(ally, ally);
        REQUIRE(mlinked.okay());
        auto linked = mlinked.release();

        auto slice = zl::raw_slice(*str, strlen(str));

        for (char c : slice)
            linked.append(c);
    }
}
} // namespace allo::tests
