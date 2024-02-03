# allo

A C++ library for making your code generic over allocation and alignment.
Inspired by the Zig stdlib.

## Benefits

- You know what memory is being allocated and how- the allocator is no longer global.
- The details of memory allocation are hidden from systems. You can choose to,
  for example, switch out a HeapAllocator with an ArenaAllocator, or a AlignedHeapAllocator
  which makes sure that everything is aligned to some minimum amount.

## Performance and Compilation Time

Allo does not use RTTI, exceptions, or virtual dispatch.
To improve compile times, it attempts to minimize use of the STL. Some headers
that are used are:

- \<type_traits\> for template magic
- \<iterator\> for stdlib iteration support (can optionally be disabled)
- \<utility\> for std::forward and std::move
- \<functional\> for std::ref

Allo uses templates liberally, but avoids forcing user code to be templated.

### Performance Benchmarks

TBD. I personally made this library for the control it offers over memory, regardless
of whether HeapAllocator is slower than malloc. I'm leaving benchmarks until someone
else uses this thing and wants that.

## C++ Standard

Allo supports C++17 and up. It was originally written for C++20 but was backported
for use on a game console. The C++20 elements are no longer present, but some
drop-in replacement features like requires-clauses are planned to be conditionally
available in the future.

## Logging and formatting

TBD. I would like to add fmtlib support for sure, but right now I just want dependencies
to be light so I'm leaving it for later.

## Directory Structure

- include/: public headers
- src/include/: private headers
- src/\*.cpp: code for use in the static lib (may be optionally inlined in the future)
