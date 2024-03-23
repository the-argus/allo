#pragma once
/// Header to be included in tests and tests only. Must be included first in the
/// file
#ifndef ALLO_HEADER_TESTING
#error attempt to compile tests without ALLO_HEADER_TESTING defined.
#endif

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>
