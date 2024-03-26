# allo

A C++ library for making your code generic over allocation and alignment.
Inspired by the Zig stdlib.

**ALLO IS STILL EXPERIMENTAL.**

## Allo Provides...

1. `scratch_allocator_t`
   - An object which allows you to allocate and to register destruction callbacks.
     Upon destruction, it frees all of its allocations and calls callbacks. Not
     threadsafe.
2. `stack_allocator_t`
   - Similar to the scratch allocator, but it allows you to remap and free
     the topmost allocation. Not threadsafe.
3. `heap_allocator_t`
   - An object which lets you arbitrarily alloc and free memory. Its advantages
     over `malloc` are that it defines exactly when it will make a syscall (by
     default, it never does) and that it frees all of its allocations upon
     destruction (as opposed to `malloc`, where you must remember to free each
     induvidual allocation). Not threadsafe.
4. `block_allocator_t`
   - An optimization of `heap_allocator_t` for when all of your allocations may
     be similarly sized. Not threadsafe.
5. `c_allocator_t`
   - A rough attempt to make `malloc` fit into the abstractions of this library.
     Its primary use is as a quick-and-easy allocator which is unbounded except
     by system memory/swap space, and can be passed into functions which might
     otherwise be intended to accept the other four.
   - Breaks the abstraction: you cannot remap allocations, nor register
     destruction callbacks. Most importantly, *there is no way to free all of
     the allocations made by this allocator, so it does nothing upon destruction.*
6. Debugging features
   - Runtime type info (rtti compiler option not required) to check if you freed
     an allocation with a different type than you originally allocated it with
   - Allocation size tracking, to ensure that you request frees as the same size
     that you allocated them with.

## Planned Features and Fixes

1. A `template <typename T> class threadsafe_t` to allow for threadsafe variants of
   the heap allocators.
2. `virtual_allocator_t`, `virtual_stack_allocator_t`, `virtual_heap_allocator_t`,
   and `virtual_threadsafe_heap_allocator_t`. A class which can be extended with
   virtual overrides to implement your own allocator and pass it to functions
   which expect an allocator from this library. Probably an antipattern, as kind of
   the whole point of Allo is that you only ever need stack, block, and heap
   allocators.
3. `debug_allocator_t` which internally uses `c_allocator_t`, but provides options
   to make certain functions always fail. Effectively a limited shortcut and could be
   achieved with the `virtual_threadsafe_heap_allocator_t`. May not be necessary.
4. An `allo::make_unique` and `allo::make_shared` for use with the heap allocators.
5. Debugging Options
   - Disable runtime allocation type info and allocation size tracking by default,
     allow enabling in debug mode or always. Encourage enabling both these features
     in debug mode, and use them in example code. However it's important to understand
     that these increase memory usage substantially before enabling them. Could cause
     bugs that only occur in release mode.
6. New Debugging Features
   - Optionally allow refcounting *all* allocations, using a global threadsafe array
     of refcounts. Would require for `zl::slice` to be overridden in debug mode to
     implement the semantics of a shared pointer. Log error details whenever the
     contents of a slice is accessed after free or before initialization. Probably
     won't make it to first release but will require significant preparation to
     ensure it can be added in later releases. Functions like `alloc_one` would need
     to be rethought, since they return a `T&` instead of a `zl::slice<T>`.
   - Optionally allow allocator constructors to take in a logging callback.
7. Allow for allocators to request new blocks of memory from their parent allocator
   instead of only trying to remap their current block. Will allow for potentially
   unbounded amounts of memory allocation, although will require some additional
   memory usage for the allocator to keep track of the blocks it owns. May be
   implemented as separate variants of the current allocators.
8. Add some sort of API to allow for saving and then restoring the state of an
   allocator, or easily registering a sub-allocator, so that procedures which
   allocate can return an error and then the caller can easily undo any allocation
   they may have performed.

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
4. Threadsafe Heap Allocator
   - A heap allocator which is additionally threadsafe and therefore provides an
	 atomic reallocation function (as opposed to other allocators, which require
	 you to alloc a new allocation, and then free the old one).

I belive heap allocators are conceptually compatible with polymorphic allocators
for the most part. If the abstraction matches up, I plan to provide a polymorphic
wrapper for these types, for use with the STL.

## Performance and Compilation Time

Allo does not use RTTI, exceptions, or virtual dispatch.
To improve compile times, it attempts to minimize use of the STL. Some headers
that are used are:

- `<type_traits>` for template magic
- `<iterator>` for stdlib iteration support (can optionally be disabled)
- `<utility>` for `std::forward`, `std::move`, and `std::in_place_t`
- `<functional>` for `std::ref`
- `<memory>` for `std::align` (only has effect on compile times in header-only mode)
- `<cstdint>` and `<cstddef>` in public headers, as well as `<cmath>` in
  header-only mode.

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
