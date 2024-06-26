#include "generic_allocator_tests.h"
#include "allo/typed_allocation.h"
#include "doctest.h"

struct CharNode;
struct LinkedString
{
    CharNode* first = nullptr;
    size_t length = 0;
    allo::abstract_allocator_t& parent;
    void append(char b);
    LinkedString() = delete;
    LinkedString(allo::abstract_allocator_t& _parent) : parent(_parent) {}
};

struct CharNode
{
    inline constexpr CharNode(char b) : contents(b), next(nullptr) {}
    CharNode* next;
    char contents;
};

void LinkedString::append(char b)
{
    auto mnewnode = allo::construct_one<CharNode>(parent, b);
    REQUIRE(mnewnode.okay());
    CharNode& newnode = mnewnode.release();
    CharNode* iter = first;
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
void allocate_object_with_linked_list(abstract_allocator_t& ally)
{
    constexpr std::array test_str{
        "hello",
        "what?",
        "the seventh son of the seventh son",
        "123456789",
    };

    for (const char* str : test_str) {
        auto mlinked = construct_one<LinkedString>(ally, ally);
        REQUIRE(mlinked.okay());
        auto linked = mlinked.release();

        auto slice = zl::raw_slice(*str, strlen(str));

        for (char c : slice)
            linked.append(c);
    }
}

bytes_t large_allocation(abstract_allocator_t& ally, size_t maxpages)
{
    auto psres = mm_get_page_size();
    REQUIRE((psres.has_value != 0));
    auto res = allo::alloc<uint8_t>(ally, psres.value * (maxpages - 1));
    REQUIRE(res.okay());
    return res.release();
}
} // namespace allo::tests
