# allo

A C++ library for making your code generic over allocation and alignment.
Inspired by the Zig stdlib.

## Benefits

- You know what memory is being allocated and how- the allocator is no longer global.
- The details of memory allocation are hidden from systems. For example:
  - switch out a `heap_allocator_t` with a `block_allocator_t` to decrease memory
    usage for many similar sized allocations.
  - switch out a `c_allocator_t` with a `stack_allocator_t` for routines which
    only perform allocation, so that the allocations become contiguous

## Why not STL polymorphic allocators?

The goal of allo is to provide a framing for programming problems where
allocators handle most of the ownership within a program. This violates RAII
(allocators, when destructed, do no call the destructors of items allocated
within them).

Additionally, allo provides a slightly different model of allocators that the
STL. Where the STL has "deleters" and the fully fledged polymorphic allocators,
allo has the following inheritance hierarchy:

1. Allocator
   - Able to allocate new memory
   - Able to register "destruction callbacks". Used for non-memory resources.
2. Stack Allocator
   - Allocator with the additionl ability to:
     - Free the most recently allocated allocation
     - Realloc the most recently allocated allocation
3. Heap Allocator
   - Stack Allocator which is able to realloc/free _any_ allocation, not
     just the most recently allocated.

I belive heap allocators are conceptually compatible with polymorphic allocators
for the most part. If the abstraction matches up, I plan to provide a polymorphic
wrapper for these types, for use with the STL.

## Performance and Compilation Time

Allo does not use RTTI, exceptions, or virtual dispatch.
To improve compile times, it attempts to minimize use of the STL. Some headers
that are used are:

- \<type_traits\> for template magic
- \<iterator\> for stdlib iteration support (can optionally be disabled)
- \<utility\> for std::forward and std::move
- \<functional\> for std::ref
- \<memory\> for std::align

Allo uses templates liberally, but avoids forcing user code to be templated.

### Performance Benchmarks

TBD. I personally made this library for the control it offers over memory, regardless
of whether `heap_allocator_t` is slower than malloc. I'm leaving benchmarks until
someone else uses this thing and wants that.

## C++ Standard

Allo supports C++17 and up. It was originally written for C++20 but was backported
for use on a game console. The C++20 elements are no longer present, but some
drop-in replacement features like requires-clauses are planned to be conditionally
available in the future.

## Logging and formatting

TBD. I would like to add fmtlib support for sure, but right now I just want dependencies
to be light so I'm leaving it for later.
