#pragma once
#include <cassert>

/// NOTE: stack usage asserts are to confirm that you are freeing in reverse
/// allocation order when using a stack. These errors are recoverable in
/// release mode
#ifndef ALLO_DISABLE_STACK_ALLOCATOR_USAGE_ASSERTS
#define ALLO_STACK_USAGE_ASSERT(expression) assert(expression)
#else
#define ALLO_STACK_USAGE_ASSERT(expression)
#endif

/// NOTE: asserts of this type have error handling paired with them, and can
/// be recovered from in release mode.
#ifndef ALLO_DISABLE_VALID_ARGUMENT_ASSERTS
#define ALLO_VALID_ARG_ASSERT(expression) assert(expression)
#else
#define ALLO_VALID_ARG_ASSERT(expression)
#endif

/// NOTE: these asserts appear in functions which explicitly have no error
/// checking, but for which it is still possible to have asserts in debug mode
#ifndef ALLO_DISABLE_UNCHECKED_ASSERTS
#define ALLO_UNCHECKED_ASSERT(expression) assert(expression)
#else
#define ALLO_UNCHECKED_ASSERT(expression)
#endif

// these asserts are intended for asserting stuff that should never break if the
// allocators are properly tested
#define ALLO_INTERNAL_ASSERT(expression) \
    assert((expression) && "internal assert has failed - file bug report")
