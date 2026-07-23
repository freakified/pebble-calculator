#pragma once

// Host-test stand-in for the Pebble SDK's <pebble.h>.
//
// The calculation engine (calc_engine*.c, calc_math.c, calc_format.c) contains
// no Pebble API calls, but calc_engine.h includes <pebble.h> transitively. When
// compiling the engine natively for the off-device test suite, this shim
// satisfies that include with just the standard-library types the headers use
// (bool, fixed-width ints, size_t). It is only ever on the include path for the
// host test build (-Itest/shim in the Makefile); the real SDK build never sees
// it.

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
