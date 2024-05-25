#pragma once
#ifdef ALLO_PROFILE_MEMORY_USAGE_TESTING
#include "allo/detail/abstracts.h"
#include "allo/structures/any_allocator.h"

namespace allo::detail {
void allocated(detail::abstract_allocator_t& self, any_allocator_t parent,
               bytes_t bytes_allocated);
void freed(detail::abstract_allocator_t& self, any_allocator_t parent,
           bytes_t bytes_freed);
} // namespace allo::detail
void begin_profile();
void end_profile();
#define ALLO_BEGIN_PROFILING() allo::detail::begin_profile()
#define ALLO_END_PROFILING() allo::detail::end_profile()
#define ALLO_CALL_USAGE_PROFILING_CALLBACK(self, parent, bytes) \
    allo::detail::allocated(self, parent, bytes)
#else
#define ALLO_BEGIN_PROFILING()
#define ALLO_END_PROFILING()
#define ALLO_CALL_USAGE_PROFILING_CALLBACK(self, parent, bytes)
#endif
