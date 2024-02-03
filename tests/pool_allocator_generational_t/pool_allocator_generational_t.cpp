#include "test_header.hpp"
// test header must be first
#include "allo/pool_allocator_generational.hpp"
#include "pool_allocator_iterable_tests.hpp"

using namespace allo;
// wrapper around generational pool allocator to make its template arguments
// match with the regular pool allocator and the iterable one
template <typename T, pool_allocator_generational_options_t options,
          typename index_t = size_t>
using generational = pool_allocator_generational_t<T, options, index_t>;
using tests =
    pool_allocator_iterable_tests<pool_allocator_generational_options_t,
                                  generational>;

TEST_SUITE("pool_allocator_generational_t")
{
    TEST_CASE("Construction and type behavior")
    {
        SUBCASE("construction with no memory allocation")
        {
            tests::construction();
        }

        SUBCASE("calls destructors of contents") { tests::destruction(); }

        SUBCASE("construction with memory allocation")
        {
            tests::construction_with_reservation();
        }
    }
    TEST_CASE("functionality")
    {
        SUBCASE("referencing allocator in other functions")
        {
            tests::use_in_other_functions();
        }
        SUBCASE("get() returns the same item that was alloced")
        {
            tests::get_functionality();
        }
        SUBCASE("no iterations for empty pool")
        {
            tests::no_iterations_for_empty();
        }
        SUBCASE("iterate over pool that has been added to")
        {
            tests::pool_that_has_been_added_to();
        }
        SUBCASE("iterate over pool that has been added to AND removed from")
        {
            tests::pool_that_has_been_added_to_and_removed_from();
        }
    }
    TEST_CASE("optimization")
    {
        SUBCASE("no copying occurs on get() or alloc_new()")
        {
            tests::no_copy();
        }
    }
}
