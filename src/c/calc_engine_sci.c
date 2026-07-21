#include "calc_engine.h"
#include "calc_engine_internal.h"
#include "calc_math.h"

#define PI_VALUE 3.14159265358979323846
#define E_VALUE  2.71828182845904523536

static double prv_to_radians(double x, bool deg_mode) {
  return deg_mode ? x * (PI_VALUE / 180.0) : x;
}

static double prv_from_radians(double x, bool deg_mode) {
  return deg_mode ? x * (180.0 / PI_VALUE) : x;
}

static double prv_factorial(double x) {
  // Defined only for non-negative integers up to 170 (171! overflows double).
  if (x < 0.0) return ERROR_VALUE;
  long long n = (long long)x;
  if ((double)n != x) return ERROR_VALUE;
  if (n > 170) return ERROR_VALUE;
  double result = 1.0;
  for (long long i = 2; i <= n; i++) {
    result *= (double)i;
  }
  return result;
}

// Decimal hours (or degrees) → H.MMSS sexagesimal encoding: 1.755 → 1.4518
// (1h 45m 18s). Work in integer centi-seconds so 59.999… seconds can't leak
// into the minutes field.
static double prv_to_hms(double x) {
  bool neg = x < 0.0;
  double a = neg ? -x : x;
  if (a > 1e12) return x; // minutes/seconds are below double precision anyway
  long long cs = (long long)(a * 360000.0 + 0.5);
  long long h = cs / 360000;
  cs %= 360000;
  long long m = cs / 6000;
  cs %= 6000; // remaining centi-seconds
  double r = (double)h + (double)m / 100.0 + (double)cs / 1000000.0;
  return neg ? -r : r;
}

// H.MMSS sexagesimal encoding → decimal hours: 1.4518 → 1.755.
static double prv_from_hms(double x) {
  bool neg = x < 0.0;
  double a = neg ? -x : x;
  if (a > 1e12) return x;
  long long h = (long long)a;
  long long v = (long long)((a - (double)h) * 1000000.0 + 0.5); // MMSScc
  long long m = v / 10000;
  double s = (double)(v % 10000) / 100.0;
  double r = (double)h + (double)m / 60.0 + s / 3600.0;
  return neg ? -r : r;
}

static double prv_apply_unary(CalcAction action, double x, bool deg_mode) {
  switch (action) {
    case CALC_ACTION_SIN:    return calc_math_sin(prv_to_radians(x, deg_mode));
    case CALC_ACTION_COS:    return calc_math_cos(prv_to_radians(x, deg_mode));
    case CALC_ACTION_TAN:    return calc_math_tan(prv_to_radians(x, deg_mode));
    case CALC_ACTION_ASIN:
      if (x < -1.0 || x > 1.0) return ERROR_VALUE;
      return prv_from_radians(calc_math_asin(x), deg_mode);
    case CALC_ACTION_ACOS:
      if (x < -1.0 || x > 1.0) return ERROR_VALUE;
      return prv_from_radians(calc_math_acos(x), deg_mode);
    case CALC_ACTION_ATAN:   return prv_from_radians(calc_math_atan(x), deg_mode);
    case CALC_ACTION_LN:
      if (x <= 0.0) return ERROR_VALUE;
      return calc_math_ln(x);
    case CALC_ACTION_LOG10:
      if (x <= 0.0) return ERROR_VALUE;
      return calc_math_log10(x);
    case CALC_ACTION_EXP:    return calc_math_exp(x);
    case CALC_ACTION_POW10:  return calc_math_pow10(x);
    case CALC_ACTION_SQRT:
      if (x < 0.0) return ERROR_VALUE;
      return calc_math_sqrt(x);
    case CALC_ACTION_SQUARE: return x * x;
    case CALC_ACTION_TO_HMS: return prv_to_hms(x);
    case CALC_ACTION_TO_H:   return prv_from_hms(x);
    case CALC_ACTION_RECIP:
      if (x == 0.0) return ERROR_VALUE;
      return 1.0 / x;
    case CALC_ACTION_FACT:   return prv_factorial(x);
    default:                 return x;
  }
}

// Apply a unary action to the current X value. Saves last_x. Updates X register
// (RPN) or current entry (Standard, replacing the value being typed).
void ce_sci_handle_unary(CalcEngine *e, CalcAction action) {
  if (e->error) return;

  double x;
  if (e->rpn_mode) {
    ce_terminate_entry(e);
    x = e->stack[3];
  } else {
    x = ce_entry_to_double(e);
  }

  double result = prv_apply_unary(action, x, e->deg_mode);
  if (ce_is_nan_or_inf(result) || ce_is_error(result)) {
    ce_set_error(e);
    return;
  }

  e->last_x = x;
  ce_double_to_entry(e, result);
  if (e->rpn_mode) {
    e->stack[3] = result;
    e->stack_lift_enabled = true;
  } else {
    e->right_committed = true;
  }
}

static void prv_handle_constant(CalcEngine *e, double value) {
  if (e->error) ce_recover_from_error(e);

  if (e->rpn_mode) {
    ce_terminate_entry(e);
    if (e->stack_lift_enabled) {
      ce_stack_push(e, e->stack[3]);
    }
    e->stack[3] = value;
    e->stack_lift_enabled = true;
  }

  ce_double_to_entry(e, value);
  if (e->rpn_mode) {
    e->stack[3] = value;
  } else {
    e->right_committed = true;
  }
}

void ce_sci_handle_pi(CalcEngine *e) {
  prv_handle_constant(e, PI_VALUE);
}

void ce_sci_handle_e(CalcEngine *e) {
  prv_handle_constant(e, E_VALUE);
}

// Every INV pairing is the key's true inverse; the one convention is
// 1/x → x! (1/x is its own inverse, so the slot is reused, TI-style).
CalcAction calc_engine_resolve_2nd(CalcAction action, bool second_active) {
  if (!second_active) return action;
  switch (action) {
    case CALC_ACTION_SIN:    return CALC_ACTION_ASIN;
    case CALC_ACTION_COS:    return CALC_ACTION_ACOS;
    case CALC_ACTION_TAN:    return CALC_ACTION_ATAN;
    case CALC_ACTION_LN:     return CALC_ACTION_EXP;
    case CALC_ACTION_LOG10:  return CALC_ACTION_POW10;
    case CALC_ACTION_SQRT:   return CALC_ACTION_SQUARE;
    case CALC_ACTION_POW:    return CALC_ACTION_NTHROOT;
    case CALC_ACTION_RECIP:  return CALC_ACTION_FACT;
    case CALC_ACTION_TO_HMS: return CALC_ACTION_TO_H;
    default:                 return action;
  }
}
