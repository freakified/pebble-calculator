#include "calc_math.h"
#include <stdbool.h>

// See calc_math.h for why this module exists. Everything below is built out
// of +, -, *, / only — no libm calls, not even for constants like INFINITY
// (produced instead via literal overflow, which is well-defined in IEEE 754
// and has been confirmed safe on device).

#define CM_PI    3.14159265358979323846
#define CM_TWOPI (2.0 * CM_PI)
#define CM_HALF_PI (CM_PI / 2.0)
#define CM_LN2   0.69314718055994530942
#define CM_LN10  2.30258509299404568402

static double prv_fabs(double x) { return x < 0.0 ? -x : x; }

// A finite double large enough to overflow to +inf on assignment/return —
// callers (ce_is_nan_or_inf) already treat "beyond representable range" as
// an error, same as an actual +inf would be.
static double prv_huge(void) { return 1e308 * 10.0; }

// floor(), used only internally for range reduction. Values already >= 2^52
// in magnitude have no fractional part representable in a double, so the
// (long long) cast — which would overflow for anything much larger — is
// only ever attempted on values that safely fit.
static double prv_floor(double x) {
  const double kIntegral = 4503599627370496.0; // 2^52
  if (x >= kIntegral || x <= -kIntegral) return x;
  long long n = (long long)x;
  double xi = (double)n;
  if (xi > x) xi -= 1.0;
  return xi;
}

static double prv_round_nearest(double x) {
  return x >= 0.0 ? prv_floor(x + 0.5) : -prv_floor(-x + 0.5);
}

// ---------------------------------------------------------------------------
// sqrt — Newton's method, range-reduced into [1, 4) so a fixed iteration
// count converges to full double precision from any positive input.
// ---------------------------------------------------------------------------

double calc_math_sqrt(double x) {
  if (x <= 0.0) return 0.0;
  double y = x;
  int exp2 = 0;
  while (y >= 4.0) { y *= 0.25; exp2++; }
  while (y < 1.0)  { y *= 4.0;  exp2--; }

  double g = 1.5;
  for (int i = 0; i < 12; i++) {
    g = 0.5 * (g + y / g);
  }

  double scale = 1.0;
  double base = (exp2 >= 0) ? 2.0 : 0.5;
  int n = exp2 >= 0 ? exp2 : -exp2;
  for (int i = 0; i < n; i++) scale *= base;
  return g * scale;
}

// ---------------------------------------------------------------------------
// exp — range-reduce x = k*ln2 + r with r in [-ln2/2, ln2/2], Taylor series
// for exp(r), then rebuild via exp(x) = exp(r) * 2^k.
// ---------------------------------------------------------------------------

static double prv_pow2i(long long k) {
  double r = 1.0;
  bool neg = k < 0;
  unsigned long long n = neg ? (unsigned long long)(-k) : (unsigned long long)k;
  double b = 2.0;
  while (n > 0) {
    if (n & 1ULL) r *= b;
    b *= b;
    n >>= 1;
  }
  return neg ? 1.0 / r : r;
}

double calc_math_exp(double x) {
  if (x > 709.78) return prv_huge();       // overflows double
  if (x < -745.13) return 0.0;             // underflows to 0

  long long k = (long long)prv_round_nearest(x / CM_LN2);
  double r = x - (double)k * CM_LN2;

  double term = 1.0;
  double sum = 1.0;
  for (int n = 1; n <= 20; n++) {
    term *= r / (double)n;
    sum += term;
  }
  return sum * prv_pow2i(k);
}

double calc_math_pow10(double x) {
  return calc_math_exp(x * CM_LN10);
}

// ---------------------------------------------------------------------------
// ln — write x = m * 2^e with m in [1, 2), then ln(m) via the fast-converging
// atanh-based series (y = (m-1)/(m+1), ln(m) = 2*(y + y^3/3 + y^5/5 + ...)).
// ---------------------------------------------------------------------------

double calc_math_ln(double x) {
  if (x <= 0.0) return -prv_huge();

  double m = x;
  int e = 0;
  while (m >= 2.0) { m *= 0.5; e++; }
  while (m < 1.0)  { m *= 2.0; e--; }

  double y = (m - 1.0) / (m + 1.0);
  double y2 = y * y;
  double term = y;
  double sum = y;
  for (int n = 1; n <= 20; n++) {
    term *= y2;
    sum += term / (2.0 * (double)n + 1.0);
  }
  double ln_m = 2.0 * sum;
  return (double)e * CM_LN2 + ln_m;
}

double calc_math_log10(double x) {
  return calc_math_ln(x) / CM_LN10;
}

// ---------------------------------------------------------------------------
// pow — integer exponents (including negative bases) use exact repeated
// squaring; everything else goes through exp(b*ln(a)), which requires a > 0.
// ---------------------------------------------------------------------------

static bool prv_is_safe_integer(double b, long long *out) {
  if (prv_fabs(b) >= 1e15) return false;
  long long n = (long long)b;
  if ((double)n != b) return false;
  *out = n;
  return true;
}

static double prv_ipow(double base, long long n) {
  bool neg_exp = n < 0;
  unsigned long long un = neg_exp ? (unsigned long long)(-n) : (unsigned long long)n;
  double result = 1.0;
  double b = base;
  while (un > 0) {
    if (un & 1ULL) result *= b;
    b *= b;
    un >>= 1;
  }
  return neg_exp ? 1.0 / result : result;
}

double calc_math_pow(double a, double b) {
  long long n;
  if (prv_is_safe_integer(b, &n)) {
    return prv_ipow(a, n);
  }
  if (a > 0.0) {
    return calc_math_exp(b * calc_math_ln(a));
  }
  if (a == 0.0) {
    return b > 0.0 ? 0.0 : prv_huge();
  }
  // Negative base, non-integer exponent: undefined for reals.
  return 0.0 / 0.0;
}

// ---------------------------------------------------------------------------
// sin/cos — reduce to [-pi, pi], then a Taylor series (15 terms comfortably
// covers double precision over that range).
// ---------------------------------------------------------------------------

static double prv_reduce_angle(double x) {
  double k = prv_round_nearest(x / CM_TWOPI);
  double r = x - k * CM_TWOPI;
  if (r > CM_PI) r -= CM_TWOPI;
  if (r < -CM_PI) r += CM_TWOPI;
  return r;
}

static double prv_sin_reduced(double r) {
  double r2 = r * r;
  double term = r;
  double sum = r;
  for (int n = 1; n <= 15; n++) {
    term *= -r2 / ((2.0 * (double)n) * (2.0 * (double)n + 1.0));
    sum += term;
  }
  return sum;
}

static double prv_cos_reduced(double r) {
  double r2 = r * r;
  double term = 1.0;
  double sum = 1.0;
  for (int n = 1; n <= 15; n++) {
    term *= -r2 / ((2.0 * (double)n - 1.0) * (2.0 * (double)n));
    sum += term;
  }
  return sum;
}

double calc_math_sin(double x) { return prv_sin_reduced(prv_reduce_angle(x)); }
double calc_math_cos(double x) { return prv_cos_reduced(prv_reduce_angle(x)); }

double calc_math_tan(double x) {
  double r = prv_reduce_angle(x);
  double c = prv_cos_reduced(r);
  if (c == 0.0) return prv_huge();
  return prv_sin_reduced(r) / c;
}

// ---------------------------------------------------------------------------
// asin/acos/atan — asin via the fast-converging series after folding |x|
// into [0, 0.5] with the identity asin(x) = pi/2 - 2*asin(sqrt((1-x)/2));
// atan via repeated tangent-half-angle reduction toward 0, then its own
// (alternating, non-fast-converging but now-tiny-argument) series.
// ---------------------------------------------------------------------------

static double prv_asin_series(double z) {
  // Valid for |z| <= ~0.5 (both call sites below guarantee this).
  double z2 = z * z;
  double term = z;
  double sum = z;
  for (int n = 0; n < 24; n++) {
    double num = (2.0 * (double)n + 1.0) * (2.0 * (double)n + 1.0);
    double den = 2.0 * ((double)n + 1.0) * (2.0 * (double)n + 3.0);
    term *= z2 * num / den;
    sum += term;
  }
  return sum;
}

double calc_math_asin(double x) {
  bool neg = x < 0.0;
  double a = neg ? -x : x;
  if (a > 1.0) a = 1.0; // guard float rounding right at the domain edge

  double result;
  if (a <= 0.5) {
    result = prv_asin_series(a);
  } else {
    double t = calc_math_sqrt((1.0 - a) * 0.5);
    result = CM_HALF_PI - 2.0 * prv_asin_series(t);
  }
  return neg ? -result : result;
}

double calc_math_acos(double x) {
  return CM_HALF_PI - calc_math_asin(x);
}

static double prv_atan_core(double x) {
  // x >= 0, x <= 1. Halve the argument (tangent half-angle identity) until
  // it's small enough for the alternating series to converge in a handful
  // of terms, tracking how many doublings are owed back at the end.
  double y = x;
  int k = 0;
  for (; k < 6; k++) {
    if (y < 0.05) break;
    y = y / (1.0 + calc_math_sqrt(1.0 + y * y));
  }

  double y2 = y * y;
  double term = y;
  double sum = y;
  for (int n = 1; n < 20; n++) {
    term *= -y2;
    sum += term / (2.0 * (double)n + 1.0);
  }
  return sum * prv_pow2i(k);
}

double calc_math_atan(double x) {
  bool neg = x < 0.0;
  double a = neg ? -x : x;
  double result = (a > 1.0) ? (CM_HALF_PI - prv_atan_core(1.0 / a))
                             : prv_atan_core(a);
  return neg ? -result : result;
}
