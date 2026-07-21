#pragma once

// Pure-C replacements for the libm transcendental functions (sqrt, sin, cos,
// tan, asin, acos, atan, log, exp, pow). Confirmed on real hardware (gabbro,
// firmware as of 2026-07): calling libm's sqrt()/sin() faults immediately on
// device (App fault, garbage LR) despite executing correctly in the emulator
// and despite plain double arithmetic (+-*/) working fine on the same
// device — the fault is specific to the linked libm entry points, not to
// floating point in general. Root cause not confirmed (likely a toolchain/
// libm mismatch for this target), so these implementations avoid libm
// entirely, using only +, -, *, / on doubles.
//
// Domain validation (e.g. rejecting sqrt of a negative, asin outside
// [-1, 1]) stays the caller's responsibility, same as it was with libm.
// Delete this module and restore the direct <math.h> calls once the
// upstream SDK/firmware issue is fixed.

double calc_math_sqrt(double x);

// Trig — x in radians.
double calc_math_sin(double x);
double calc_math_cos(double x);
double calc_math_tan(double x);

// Inverse trig — result in radians. Caller must ensure |x| <= 1 for asin/acos.
double calc_math_asin(double x);
double calc_math_acos(double x);
double calc_math_atan(double x);

// Caller must ensure x > 0.
double calc_math_ln(double x);
double calc_math_log10(double x);

double calc_math_exp(double x);
double calc_math_pow10(double x);

// General a^b, including negative a with an integer b (e.g. (-2)^3 == -8).
// Negative a with a non-integer b is mathematically undefined; returns NaN.
double calc_math_pow(double a, double b);
