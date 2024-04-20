#include "allo/memory_map.h"
#include "test_header.h"

constexpr size_t bytes_in_gb = 1000000000;

TEST_SUITE("memory mapping alloc")
{
    TEST_CASE("get_page_size")
    {
        SUBCASE("page size seems reasonable")
        {
            auto res = mm_get_page_size();
            REQUIRE((res.has_value == 1));
            REQUIRE((res.value > 32));
            REQUIRE((res.value < bytes_in_gb));
        }
    }
    TEST_CASE("functionality of commit and reserve")
    {
        // this test fails on systems with less than a GB of free memory
        SUBCASE("commit and then reserve the same amount")
        {
            auto pagesize_res = mm_get_page_size();
            REQUIRE((pagesize_res.has_value == 1));
            size_t pagesize = pagesize_res.value;
            // allocate min 1GB
            auto num_pages = (bytes_in_gb / pagesize) + 1;
            auto res = mm_reserve_pages(nullptr, num_pages);
            REQUIRE(res.code == 0);
            REQUIRE(res.data != nullptr);
            int32_t commit_res = mm_commit_pages(res.data, num_pages);
            REQUIRE(commit_res == 0);
        }
        // you can commit more than you reserved, it will just overwrite the
        // reservation
        SUBCASE("committing more than was reserved")
        {
            auto res = mm_reserve_pages(nullptr, 1);
            REQUIRE(res.code == 0);
            REQUIRE(res.data != nullptr);
            int32_t commit_res = mm_commit_pages(res.data, 2);
            REQUIRE(commit_res == 0);
        }
        SUBCASE("writing to comitted memory")
        {
            auto res = mm_reserve_pages(nullptr, 2);
            REQUIRE(res.code == 0);
            int32_t commit_res = mm_commit_pages(res.data, 2);
            REQUIRE(commit_res == 0);
            auto *data = (uint8_t *)res.data;
            auto pagesize_res = mm_get_page_size();
            REQUIRE((pagesize_res.has_value == 1));
            size_t pagesize = pagesize_res.value;
            for (size_t i = 0; i < pagesize_res.value; ++i) {
                data[i] = (uint8_t)i;
            }
        }
        SUBCASE("reserving, comitting, and then re-comitting")
        {
            auto res = mm_reserve_pages(nullptr, 10);
            REQUIRE(res.code == 0);
            int32_t commit_res = mm_commit_pages(res.data, 2);
            REQUIRE(commit_res == 0);
            commit_res = mm_commit_pages(res.data, 8);
            REQUIRE(commit_res == 0);
        }
    }
    TEST_CASE("errors returned")
    {
        // this test creates a false negative on systems with 100GB of free
        // memory
        // NOTE: this test requires that committing 100GB of memory should fail.
        // however it seems to *succeed* on my linux machine. I think this is
        // because, if youre on a typical linux desktop system, kernel
        // overcommit is usually turned on and allows for committing but not
        // writing to the virtual memory.
        /*
        SUBCASE("over-committing but not over-reserving")
        {
            auto pagesize_res = mm_get_page_size();
            REQUIRE((pagesize_res.has_value == 1));
            size_t pagesize = pagesize_res.value;
            // allocate min 10GB
            auto num_pages = ((100UL * bytes_in_gb) / pagesize) + 1;
            auto res = mm_reserve_pages(nullptr, num_pages);
            REQUIRE(res.code == 0);
            REQUIRE(res.data != nullptr);
           int32_t commit_res = mm_commit_pages(res.data, num_pages);
            REQUIRE(commit_res == -1);
        }
        */
        SUBCASE("trying to reserve 0")
        {
            auto res = mm_reserve_pages(nullptr, 0);
            REQUIRE(res.code != 0);
            REQUIRE(res.bytes == 0);
            REQUIRE(res.data == nullptr);
        }
        SUBCASE("commit a nullptr")
        {
            int32_t res = mm_commit_pages(nullptr, 1);
            REQUIRE(res == -1);
            res = mm_commit_pages(nullptr, 0);
            REQUIRE(res != 0);
        }
    }
}
