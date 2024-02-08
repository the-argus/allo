#pragma once
// Defines the ALLO_ABORT() macro which throws an exception in debug mode or
// just calls std::abort in release mode.

#ifdef ALLO_HEADER_TESTING
#include <exception>
namespace reserve {
class _abort_exception : std::exception {
public:
  // NOLINTNEXTLINE
  char *what() { return const_cast<char *>("Program failure."); }
};
} // namespace reserve
#define ALLO_ABORT() throw ::reserve::_abort_exception()
#else
#include <cstdlib>
#define ALLO_ABORT() std::abort()
#endif
