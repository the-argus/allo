#pragma once
#include "testing/abort.hpp"
/// Header to be included in tests and tests only. Must be included first in the
/// file
#ifndef TESTING
#error attempt to compile tests without TESTING defined.
#endif

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#define REQUIREABORTS(operation)                       \
    {                                                  \
        bool status = false;                           \
        try {                                          \
            operation;                                 \
        } catch (const reserve::_abort_exception &e) { \
            status = true;                             \
        }                                              \
        REQUIRE(status);                               \
    }
