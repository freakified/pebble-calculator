#include "calc_engine.h"
#include "calc_format.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

#define PI_VALUE 3.14159265358979323846
#define E_VALUE  2.71828182845904523536

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static double prv_fabs(double x) {
  return x < 0.0 ? -x : x;
}

#define ERROR_VALUE 1e18
static bool prv_is_error(double x) {
  return prv_fabs(x) > 9.999e15;
}

// NaN/Infinity detection without relying on isnan/isinf macros — anything
// outside the doubles we can format is treated as an error.
static bool prv_is_nan_or_inf(double x) {
  return !(x == x) || prv_fabs(x) > 1e300;
}

static int prv_count_digits(const char *s, int len) {
  int count = 0;
  for (int i = 0; i < len; i++) {
    if (s[i] >= '0' && s[i] <= '9') count++;
  }
  return count;
}

static void prv_clear_entry(CalcEngine *e) {
  e->entry[0] = '0';
  e->entry[1] = '\0';
  e->entry_len = 1;
  e->has_dot = false;
  e->entering = false;
}

static double prv_entry_to_double(CalcEngine *e) {
  if (e->error || e->entry[0] == 'E') return 0.0;
  return calc_format_parse(e->entry, e->entry_len);
}

static void prv_double_to_entry(CalcEngine *e, double val) {
  bool overflow = false;
  e->entry_len = calc_format_double(val, e->entry, &overflow);
  e->has_dot = (memchr(e->entry, '.', e->entry_len) != NULL);
  e->entering = false;
  if (overflow) e->error = true;
}

static void prv_set_error(CalcEngine *e) {
  e->error = true;
  snprintf(e->entry, sizeof(e->entry), "Error");
  e->entry_len = 5;
  e->entering = false;
}

// Recover from error state in response to a digit/dot keypress: standard mode
// fully re-initializes; RPN keeps the stack so the user can resume from the
// pre-error operands (T/Z/Y intact, X gets the new entry).
static void prv_recover_from_error(CalcEngine *e) {
  e->error = false;
  if (!e->rpn_mode) {
    // Preserve sticky scientific state across error-recovery init.
    int   page = e->page;
    bool  deg  = e->deg_mode;
    double mem = e->memory;
    calc_engine_init(e);
    e->page = page;
    e->deg_mode = deg;
    e->memory = mem;
  } else {
    prv_clear_entry(e);
  }
}

static double prv_apply_op(double left, CalcOp op, double right) {
  switch (op) {
    case CALC_OP_ADD:      return left + right;
    case CALC_OP_SUBTRACT: return left - right;
    case CALC_OP_MULTIPLY: return left * right;
    case CALC_OP_DIVIDE:   return right != 0.0 ? left / right : ERROR_VALUE;
    case CALC_OP_POWER:    return pow(left, right);
    case CALC_OP_NTHROOT:  return right != 0.0 ? pow(left, 1.0 / right) : ERROR_VALUE;
    default:               return right;
  }
}

static CalcOp prv_action_to_op(CalcAction action) {
  switch (action) {
    case CALC_ACTION_ADD:      return CALC_OP_ADD;
    case CALC_ACTION_SUBTRACT: return CALC_OP_SUBTRACT;
    case CALC_ACTION_MULTIPLY: return CALC_OP_MULTIPLY;
    case CALC_ACTION_DIVIDE:   return CALC_OP_DIVIDE;
    case CALC_ACTION_POW:      return CALC_OP_POWER;
    case CALC_ACTION_NTHROOT:  return CALC_OP_NTHROOT;
    default:                   return CALC_OP_NONE;
  }
}

static const char *prv_op_to_str(CalcOp op) {
  switch (op) {
    case CALC_OP_ADD:      return "+";
    case CALC_OP_SUBTRACT: return "-";
    case CALC_OP_MULTIPLY: return "x";
    case CALC_OP_DIVIDE:   return "/";
    case CALC_OP_POWER:    return "^";
    case CALC_OP_NTHROOT:  return "rt";
    default:               return "";
  }
}

static int prv_op_precedence(CalcOp op) {
  switch (op) {
    case CALC_OP_ADD:
    case CALC_OP_SUBTRACT: return 1;
    case CALC_OP_MULTIPLY:
    case CALC_OP_DIVIDE:   return 2;
    case CALC_OP_POWER:
    case CALC_OP_NTHROOT:  return 3;
    default:               return 0;
  }
}

static bool prv_op_is_right_assoc(CalcOp op) {
  return op == CALC_OP_POWER || op == CALC_OP_NTHROOT;
}

// RPN stack helpers
static void prv_stack_push(CalcEngine *e, double val) {
  // Shift up: T is lost, Z←Y, Y←X, X←val
  e->stack[0] = e->stack[1]; // T ← Z
  e->stack[1] = e->stack[2]; // Z ← Y
  e->stack[2] = e->stack[3]; // Y ← X
  e->stack[3] = val;         // X ← val
}

static void prv_stack_drop(CalcEngine *e) {
  // X ← Y, Y ← Z, Z ← T, T duplicates
  e->stack[3] = e->stack[2];
  e->stack[2] = e->stack[1];
  e->stack[1] = e->stack[0];
  // T stays
}

// Commit any in-progress digit entry into X, and treat that just-entered value
// as a result for stack-lift purposes (next push will lift). RPN-only.
static void prv_terminate_entry(CalcEngine *e) {
  if (e->entering) {
    e->stack[3] = prv_entry_to_double(e);
    e->entering = false;
    e->stack_lift_enabled = true;
  }
}

// ---------------------------------------------------------------------------
// Digit / dot entry
// ---------------------------------------------------------------------------

static void prv_handle_digit(CalcEngine *e, int digit) {
  if (e->error) prv_recover_from_error(e);

  // A typed digit always starts a fresh right operand, replacing any committed
  // result (unary/constant/MR) sitting in entry.
  e->right_committed = false;

  if (e->rpn_mode && !e->entering && e->stack_lift_enabled) {
    // Lift the stack before starting new entry
    prv_stack_push(e, prv_entry_to_double(e));
  }

  if (!e->entering) {
    // Start fresh entry
    e->entry[0] = '0' + digit;
    e->entry[1] = '\0';
    e->entry_len = 1;
    e->has_dot = false;
    e->entering = true;
    return;
  }

  // Already entering — append digit
  // Cap digits to what fits on display (sign takes one digit slot)
  int max_digits = (e->entry[0] == '-') ? (CALC_FORMAT_MAX_DIGITS - 1) : CALC_FORMAT_MAX_DIGITS;
  if (prv_count_digits(e->entry, e->entry_len) >= max_digits) return;

  // Don't allow leading zeros (unless after decimal)
  if (e->entry_len == 1 && e->entry[0] == '0' && !e->has_dot && digit == 0) {
    return;
  }
  if (e->entry_len == 1 && e->entry[0] == '0' && !e->has_dot) {
    // Replace the leading zero
    e->entry[0] = '0' + digit;
    return;
  }

  // Handle "-0"
  if (e->entry_len == 2 && e->entry[0] == '-' && e->entry[1] == '0' && !e->has_dot && digit == 0) {
    return;
  }
  if (e->entry_len == 2 && e->entry[0] == '-' && e->entry[1] == '0' && !e->has_dot) {
    e->entry[1] = '0' + digit;
    return;
  }

  e->entry[e->entry_len] = '0' + digit;
  e->entry_len++;
  e->entry[e->entry_len] = '\0';
}

static void prv_handle_dot(CalcEngine *e) {
  if (e->error) prv_recover_from_error(e);

  e->right_committed = false;

  if (e->rpn_mode && !e->entering && e->stack_lift_enabled) {
    prv_stack_push(e, prv_entry_to_double(e));
  }

  if (!e->entering) {
    e->entry[0] = '0';
    e->entry[1] = '.';
    e->entry[2] = '\0';
    e->entry_len = 2;
    e->has_dot = true;
    e->entering = true;
    return;
  }

  if (e->has_dot) return;
  if (e->entry_len >= CALC_DISPLAY_MAX - 1) return; // buffer safety cap

  e->entry[e->entry_len] = '.';
  e->entry_len++;
  e->entry[e->entry_len] = '\0';
  e->has_dot = true;
}

// ---------------------------------------------------------------------------
// Standard mode operations
// ---------------------------------------------------------------------------

// Pop the top frame and apply its op to the current entry, writing the result
// back into entry. Returns false (and sets engine error) on math failure.
static bool prv_op_stack_fold_top(CalcEngine *e) {
  CalcOpFrame top = e->op_stack[e->op_stack_size - 1];
  e->op_stack_size--;

  double right = prv_entry_to_double(e);
  double result = prv_apply_op(top.value, top.op, right);

  if (prv_is_nan_or_inf(result) || prv_is_error(result)) {
    prv_set_error(e);
    return false;
  }
  prv_double_to_entry(e, result);
  return true;
}

// Snapshot the post-'=' tape view BEFORE the drain runs, while frames and the
// right operand (in entry) are still intact. Format: "v0 op0 v1 op1 ... R =".
static void prv_snapshot_expression(CalcEngine *e) {
  char *buf = e->last_expression;
  int buf_size = (int)sizeof(e->last_expression);
  buf[0] = '\0';
  int written = 0;
  for (int i = 0; i < e->op_stack_size; i++) {
    CalcOpFrame f = e->op_stack[i];
    char val_buf[CALC_FORMAT_BUF_SIZE];
    calc_format_double(f.value, val_buf, NULL);
    int n = snprintf(buf + written, buf_size - written, "%s %s ",
                     val_buf, prv_op_to_str(f.op));
    if (n < 0 || n >= buf_size - written) { buf[0] = '\0'; return; }
    written += n;
  }
  char r_buf[CALC_FORMAT_BUF_SIZE];
  calc_format_double(prv_entry_to_double(e), r_buf, NULL);
  int n = snprintf(buf + written, buf_size - written, "%s =", r_buf);
  if (n < 0 || n >= buf_size - written) buf[0] = '\0';
}

static void prv_standard_evaluate(CalcEngine *e) {
  if (e->op_stack_size == 0) return;
  prv_snapshot_expression(e);
  while (e->op_stack_size > 0) {
    if (!prv_op_stack_fold_top(e)) {
      // Drain failed (overflow / NaN). Don't show a partial-state snapshot.
      e->last_expression[0] = '\0';
      return;
    }
  }
  e->right_committed = false;
}

static void prv_standard_operator(CalcEngine *e, CalcAction action) {
  if (e->error) return;

  CalcOp new_op = prv_action_to_op(action);

  // No operand was typed since the last op press (or =), and entry doesn't
  // hold a committed result either — the user is just changing their mind
  // about which operator to apply. Replace the top frame's op (or push a fresh
  // frame if the stack is empty).
  if (!e->entering && !e->right_committed) {
    if (e->op_stack_size > 0) {
      e->op_stack[e->op_stack_size - 1].op = new_op;
    } else {
      e->op_stack[0].value = prv_entry_to_double(e);
      e->op_stack[0].op    = new_op;
      e->op_stack_size = 1;
    }
    return;
  }

  // Fold any deferred ops whose precedence binds tighter than the new op.
  // For right-associative new ops (^, nthroot), only fold when strictly tighter.
  int new_prec = prv_op_precedence(new_op);
  bool right_assoc = prv_op_is_right_assoc(new_op);
  while (e->op_stack_size > 0) {
    int top_prec = prv_op_precedence(e->op_stack[e->op_stack_size - 1].op);
    bool should_fold = (top_prec > new_prec) ||
                       (top_prec == new_prec && !right_assoc);
    if (!should_fold) break;
    if (!prv_op_stack_fold_top(e)) return;
  }

  if (e->op_stack_size == CALC_OP_STACK_DEPTH) { prv_set_error(e); return; }
  e->op_stack[e->op_stack_size].value = prv_entry_to_double(e);
  e->op_stack[e->op_stack_size].op    = new_op;
  e->op_stack_size++;
  e->entering = false;
  e->right_committed = false;
}

// ---------------------------------------------------------------------------
// RPN mode operations
// ---------------------------------------------------------------------------

static void prv_rpn_enter(CalcEngine *e) {
  double val = prv_entry_to_double(e);
  prv_stack_push(e, val);
  e->stack[3] = val; // X = same value (classic ENTER behavior)
  prv_double_to_entry(e, val);
  e->stack_lift_enabled = false; // next digit replaces X, no lift
}

static void prv_rpn_operator(CalcEngine *e, CalcAction action) {
  if (e->error) return;

  // Finalize any in-progress entry into X
  double x = prv_entry_to_double(e);
  e->stack[3] = x;

  // Pop X and Y
  double x_val = e->stack[3];
  double y_val = e->stack[2];

  CalcOp op = prv_action_to_op(action);
  double result = prv_apply_op(y_val, op, x_val);

  if (prv_is_nan_or_inf(result) || prv_is_error(result)) {
    prv_set_error(e);
    return;
  }

  e->last_x = x_val;

  // Drop the stack (Y consumed), push result into X
  e->stack[2] = e->stack[1]; // Y ← Z
  e->stack[1] = e->stack[0]; // Z ← T
  // T stays (duplicates)
  e->stack[3] = result;      // X ← result

  prv_double_to_entry(e, result);
  e->stack_lift_enabled = true;
}

static void prv_rpn_swap(CalcEngine *e) {
  if (e->entering) {
    e->stack[3] = prv_entry_to_double(e);
    e->entering = false;
  }
  double tmp = e->stack[3];
  e->stack[3] = e->stack[2];
  e->stack[2] = tmp;
  prv_double_to_entry(e, e->stack[3]);
  e->stack_lift_enabled = true;
}

// In RPN mode, BACKSPACE/CLEAR when there's no entry in progress clears X
// (HP-style "CLx") rather than dropping the stack — Y/Z/T are preserved.
static void prv_rpn_clear_x(CalcEngine *e) {
  prv_clear_entry(e);
  e->stack[3] = 0.0;
  e->stack_lift_enabled = false;
}

// ---------------------------------------------------------------------------
// Scientific — unary functions
// ---------------------------------------------------------------------------

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
  if ((double)n != x) return ERROR_VALUE; // not an integer
  if (n > 170) return ERROR_VALUE;
  double result = 1.0;
  for (long long i = 2; i <= n; i++) {
    result *= (double)i;
  }
  return result;
}

static double prv_apply_unary(CalcAction action, double x, bool deg_mode) {
  switch (action) {
    case CALC_ACTION_SIN:    return sin(prv_to_radians(x, deg_mode));
    case CALC_ACTION_COS:    return cos(prv_to_radians(x, deg_mode));
    case CALC_ACTION_TAN:    return tan(prv_to_radians(x, deg_mode));
    case CALC_ACTION_ASIN:
      if (x < -1.0 || x > 1.0) return ERROR_VALUE;
      return prv_from_radians(asin(x), deg_mode);
    case CALC_ACTION_ACOS:
      if (x < -1.0 || x > 1.0) return ERROR_VALUE;
      return prv_from_radians(acos(x), deg_mode);
    case CALC_ACTION_ATAN:   return prv_from_radians(atan(x), deg_mode);
    case CALC_ACTION_LN:
      if (x <= 0.0) return ERROR_VALUE;
      return log(x);
    case CALC_ACTION_LOG10:
      if (x <= 0.0) return ERROR_VALUE;
      return log(x) / log(10.0);
    case CALC_ACTION_EXP:    return exp(x);
    case CALC_ACTION_POW10:  return pow(10.0, x);
    case CALC_ACTION_SQRT:
      if (x < 0.0) return ERROR_VALUE;
      return sqrt(x);
    case CALC_ACTION_SQUARE: return x * x;
    case CALC_ACTION_CUBE:   return x * x * x;
    case CALC_ACTION_CBRT: {
      // No standard cbrt on Pebble; pow handles negatives via sign extraction.
      if (x < 0.0) return -pow(-x, 1.0 / 3.0);
      return pow(x, 1.0 / 3.0);
    }
    case CALC_ACTION_RECIP:
      if (x == 0.0) return ERROR_VALUE;
      return 1.0 / x;
    case CALC_ACTION_FACT:   return prv_factorial(x);
    default:                 return x;
  }
}

// Apply a unary action to the current X value. Saves last_x. Updates X register
// (RPN) or current entry (Standard, replacing the value being typed).
static void prv_handle_unary(CalcEngine *e, CalcAction action) {
  if (e->error) return;

  double x;
  if (e->rpn_mode) {
    prv_terminate_entry(e);
    x = e->stack[3];
  } else {
    x = prv_entry_to_double(e);
  }

  double result = prv_apply_unary(action, x, e->deg_mode);
  if (prv_is_nan_or_inf(result) || prv_is_error(result)) {
    prv_set_error(e);
    return;
  }

  e->last_x = x;
  prv_double_to_entry(e, result);
  if (e->rpn_mode) {
    e->stack[3] = result;
    e->stack_lift_enabled = true;
  } else {
    e->right_committed = true;
  }
}

static void prv_handle_constant(CalcEngine *e, double value) {
  if (e->error) prv_recover_from_error(e);

  if (e->rpn_mode) {
    prv_terminate_entry(e);
    if (e->stack_lift_enabled) {
      prv_stack_push(e, e->stack[3]);
    }
    e->stack[3] = value;
    e->stack_lift_enabled = true;
  }

  prv_double_to_entry(e, value);
  if (e->rpn_mode) {
    e->stack[3] = value;
  } else {
    e->right_committed = true;
  }
}

// ---------------------------------------------------------------------------
// Memory operations
// ---------------------------------------------------------------------------

static void prv_handle_memory(CalcEngine *e, CalcAction action) {
  if (e->error) return;

  // Read current X value (committing any in-progress entry).
  double x;
  if (e->rpn_mode) {
    prv_terminate_entry(e);
    x = e->stack[3];
  } else {
    x = prv_entry_to_double(e);
  }

  switch (action) {
    case CALC_ACTION_M_PLUS:    e->memory += x; break;
    case CALC_ACTION_M_MINUS:   e->memory -= x; break;
    case CALC_ACTION_M_CLEAR:   e->memory = 0.0; break;
    case CALC_ACTION_M_RECALL:
      // Recall pushes memory onto X with stack lift — typed values that were
      // just terminated are preserved on Y (prv_terminate_entry sets lift=true).
      if (e->rpn_mode) {
        if (e->stack_lift_enabled) {
          prv_stack_push(e, e->stack[3]);
        }
        e->stack[3] = e->memory;
        e->stack_lift_enabled = true;
      }
      prv_double_to_entry(e, e->memory);
      if (e->rpn_mode) e->stack[3] = e->memory;
      else e->right_committed = true;
      break;
    default: break;
  }
}

// ---------------------------------------------------------------------------
// RPN stack operations
// ---------------------------------------------------------------------------

static void prv_handle_stack_op(CalcEngine *e, CalcAction action) {
  if (e->error) return;
  if (!e->rpn_mode) return;

  prv_terminate_entry(e);

  switch (action) {
    case CALC_ACTION_ROLL_DOWN: {
      // X→T, Y→X, Z→Y, T→Z (rotate down)
      double t = e->stack[3];
      e->stack[3] = e->stack[2];
      e->stack[2] = e->stack[1];
      e->stack[1] = e->stack[0];
      e->stack[0] = t;
      break;
    }
    case CALC_ACTION_ROLL_UP: {
      // T→X (wraps), X→Y, Y→Z, Z→T (rotate up)
      double t = e->stack[0];
      e->stack[0] = e->stack[1];
      e->stack[1] = e->stack[2];
      e->stack[2] = e->stack[3];
      e->stack[3] = t;
      break;
    }
    case CALC_ACTION_DROP:
      e->last_x = e->stack[3];
      prv_stack_drop(e);
      break;
    case CALC_ACTION_STACK_CLEAR:
      e->stack[0] = e->stack[1] = e->stack[2] = e->stack[3] = 0.0;
      break;
    case CALC_ACTION_LAST_X:
      // Push last_x onto stack with lift.
      if (e->stack_lift_enabled) {
        prv_stack_push(e, e->stack[3]);
      }
      e->stack[3] = e->last_x;
      break;
    default: break;
  }

  prv_double_to_entry(e, e->stack[3]);
  e->stack_lift_enabled = true;
}

// ---------------------------------------------------------------------------
// 2nd-modifier resolution
// ---------------------------------------------------------------------------

CalcAction calc_engine_resolve_2nd(CalcAction action, bool second_active) {
  if (!second_active) return action;
  switch (action) {
    case CALC_ACTION_SIN:    return CALC_ACTION_ASIN;
    case CALC_ACTION_COS:    return CALC_ACTION_ACOS;
    case CALC_ACTION_TAN:    return CALC_ACTION_ATAN;
    case CALC_ACTION_LN:     return CALC_ACTION_EXP;
    case CALC_ACTION_LOG10:  return CALC_ACTION_POW10;
    case CALC_ACTION_SQRT:   return CALC_ACTION_CUBE;
    case CALC_ACTION_SQUARE: return CALC_ACTION_CBRT;
    case CALC_ACTION_POW:    return CALC_ACTION_NTHROOT;
    case CALC_ACTION_RECIP:  return CALC_ACTION_FACT;
    default:                 return action;
  }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void calc_engine_init(CalcEngine *engine) {
  memset(engine, 0, sizeof(CalcEngine));
  prv_clear_entry(engine);
  engine->stack_lift_enabled = false;
  engine->page = CALC_PAGE_BASIC;
  engine->second_active = false;
  engine->deg_mode = true;  // most users expect degrees by default
  engine->memory = 0.0;
  engine->last_x = 0.0;
}

void calc_engine_set_rpn_mode(CalcEngine *engine, bool rpn) {
  // Preserve scientific-state across mode toggle so flipping RPN doesn't
  // wipe DEG/RAD or the memory register.
  bool deg = engine->deg_mode;
  double mem = engine->memory;
  calc_engine_init(engine);
  engine->rpn_mode = rpn;
  engine->deg_mode = deg;
  engine->memory = mem;
}

void calc_engine_handle_action(CalcEngine *engine, CalcAction action) {
  // 2nd-modifier toggle is handled here so the modifier flag is set before
  // the action is resolved against it (no resolve for the toggle itself).
  if (action == CALC_ACTION_2ND_TOGGLE) {
    engine->second_active = !engine->second_active;
    return;
  }

  // Page cycle is independent of mode/state.
  if (action == CALC_ACTION_PAGE_NEXT) {
    engine->page = (engine->page + 1) % CALC_PAGE_COUNT;
    engine->second_active = false;
    return;
  }

  // NOOP — used for empty button slots; explicitly do nothing (and don't
  // clear 2nd-modifier so a stray empty press doesn't deactivate the user's
  // pending 2nd selection).
  if (action == CALC_ACTION_NOOP) return;

  // Resolve any 2nd-modifier remap. The flag is sticky on the engine and
  // auto-clears below after dispatch.
  CalcAction resolved = calc_engine_resolve_2nd(action, engine->second_active);
  bool clear_second_after = engine->second_active;

  // Any non-modifier action invalidates the post-'=' tape view: the user is
  // either starting a new computation or continuing from the result, so the
  // snapshot no longer matches what's on screen. Equals re-populates after.
  engine->last_expression[0] = '\0';

  // Backspace / Clear
  if (resolved == CALC_ACTION_BACKSPACE || resolved == CALC_ACTION_CLEAR) {
    if (engine->error) {
      if (engine->rpn_mode) {
        engine->error = false;
        prv_rpn_clear_x(engine);
      } else {
        prv_recover_from_error(engine);
      }
      goto done;
    }

    if (engine->entering) {
      if (engine->entry_len <= 1) {
        prv_clear_entry(engine);
        if (engine->rpn_mode) engine->stack[3] = 0.0;
        goto done;
      }
      if (engine->entry[engine->entry_len - 1] == '.') {
        engine->has_dot = false;
      }
      engine->entry_len--;
      engine->entry[engine->entry_len] = '\0';
      if (engine->entry_len == 1 && engine->entry[0] == '-') {
        prv_clear_entry(engine);
        if (engine->rpn_mode) engine->stack[3] = 0.0;
        goto done;
      }
      if (engine->rpn_mode) {
        engine->stack[3] = prv_entry_to_double(engine);
      }
      goto done;
    }

    // Not entering, no error
    if (engine->rpn_mode) {
      prv_rpn_clear_x(engine);
    } else if (resolved == CALC_ACTION_CLEAR) {
      // Preserve sticky scientific state across CLEAR.
      int   page = engine->page;
      bool  deg  = engine->deg_mode;
      double mem = engine->memory;
      calc_engine_init(engine);
      engine->page = page;
      engine->deg_mode = deg;
      engine->memory = mem;
    }
    goto done;
  }

  // Digits
  if (resolved <= CALC_ACTION_DIGIT_9) {
    prv_handle_digit(engine, (int)resolved - (int)CALC_ACTION_DIGIT_0);
    if (engine->rpn_mode) {
      engine->stack[3] = prv_entry_to_double(engine);
    }
    goto done;
  }

  // Decimal point
  if (resolved == CALC_ACTION_DOT) {
    prv_handle_dot(engine);
    goto done;
  }

  // Negate
  if (resolved == CALC_ACTION_NEGATE) {
    if (engine->error) goto done;
    if (engine->entering) {
      if (engine->entry[0] == '-') {
        memmove(engine->entry, engine->entry + 1, engine->entry_len);
        engine->entry_len--;
      } else {
        if (engine->entry_len < CALC_DISPLAY_MAX - 1) {
          memmove(engine->entry + 1, engine->entry, engine->entry_len + 1);
          engine->entry[0] = '-';
          engine->entry_len++;
        }
      }
    } else {
      double val = prv_entry_to_double(engine);
      val = -val;
      prv_double_to_entry(engine, val);
    }
    if (engine->rpn_mode) {
      engine->stack[3] = prv_entry_to_double(engine);
    }
    goto done;
  }

  // Binary operators (basic + power/nth-root)
  if (resolved == CALC_ACTION_ADD || resolved == CALC_ACTION_SUBTRACT ||
      resolved == CALC_ACTION_MULTIPLY || resolved == CALC_ACTION_DIVIDE ||
      resolved == CALC_ACTION_POW || resolved == CALC_ACTION_NTHROOT) {
    if (engine->rpn_mode) {
      prv_rpn_operator(engine, resolved);
    } else {
      prv_standard_operator(engine, resolved);
    }
    goto done;
  }

  // Equals / Enter
  if (resolved == CALC_ACTION_EQUALS) {
    if (engine->rpn_mode) {
      prv_rpn_enter(engine);
    } else {
      prv_standard_evaluate(engine);
    }
    goto done;
  }

  if (resolved == CALC_ACTION_ENTER) {
    prv_rpn_enter(engine);
    goto done;
  }

  // RPN swap
  if (resolved == CALC_ACTION_SWAP) {
    if (engine->rpn_mode) prv_rpn_swap(engine);
    goto done;
  }

  // Unary scientific
  if (resolved == CALC_ACTION_SIN || resolved == CALC_ACTION_COS ||
      resolved == CALC_ACTION_TAN || resolved == CALC_ACTION_ASIN ||
      resolved == CALC_ACTION_ACOS || resolved == CALC_ACTION_ATAN ||
      resolved == CALC_ACTION_LN || resolved == CALC_ACTION_LOG10 ||
      resolved == CALC_ACTION_EXP || resolved == CALC_ACTION_POW10 ||
      resolved == CALC_ACTION_SQRT || resolved == CALC_ACTION_SQUARE ||
      resolved == CALC_ACTION_CUBE || resolved == CALC_ACTION_CBRT ||
      resolved == CALC_ACTION_RECIP || resolved == CALC_ACTION_FACT) {
    prv_handle_unary(engine, resolved);
    goto done;
  }

  // Constants
  if (resolved == CALC_ACTION_PI) {
    prv_handle_constant(engine, PI_VALUE);
    goto done;
  }
  if (resolved == CALC_ACTION_E) {
    prv_handle_constant(engine, E_VALUE);
    goto done;
  }

  // Memory
  if (resolved == CALC_ACTION_M_PLUS || resolved == CALC_ACTION_M_MINUS ||
      resolved == CALC_ACTION_M_RECALL || resolved == CALC_ACTION_M_CLEAR) {
    prv_handle_memory(engine, resolved);
    goto done;
  }

  // RPN stack ops
  if (resolved == CALC_ACTION_ROLL_DOWN || resolved == CALC_ACTION_ROLL_UP ||
      resolved == CALC_ACTION_DROP || resolved == CALC_ACTION_STACK_CLEAR ||
      resolved == CALC_ACTION_LAST_X) {
    prv_handle_stack_op(engine, resolved);
    goto done;
  }

done:
  if (clear_second_after) engine->second_active = false;
}

const char *calc_engine_get_x_display(CalcEngine *engine) {
  return engine->entry;
}

void calc_engine_get_stack_display(CalcEngine *engine, int reg, char *buf, int buf_size) {
  if (reg < 0 || reg > 2 || buf_size < CALC_FORMAT_BUF_SIZE) {
    buf[0] = '\0';
    return;
  }
  calc_format_double(engine->stack[reg], buf, NULL);
}

void calc_engine_get_secondary_display(CalcEngine *engine, char *buf, int buf_size) {
  if (engine->rpn_mode) {
    calc_engine_get_stack_display(engine, 2, buf, buf_size);
    return;
  }

  // Standard mode: render every deferred (operand, operator) frame so the
  // user can see what's queued behind the active term. The renderer right-
  // aligns with trailing ellipsis, so deep stacks elide oldest-first.
  buf[0] = '\0';
  if (engine->op_stack_size == 0) {
    // No active stack: show the post-'=' tape snapshot if one is set, so
    // the user can see what they just calculated alongside the result.
    if (engine->last_expression[0] != '\0') {
      snprintf(buf, buf_size, "%s", engine->last_expression);
    }
    return;
  }

  int written = 0;
  for (int i = 0; i < engine->op_stack_size; i++) {
    CalcOpFrame f = engine->op_stack[i];
    char val_buf[CALC_FORMAT_BUF_SIZE];
    calc_format_double(f.value, val_buf, NULL);
    int n = snprintf(buf + written, buf_size - written,
                     (i == 0) ? "%s %s" : " %s %s",
                     val_buf, prv_op_to_str(f.op));
    if (n < 0 || n >= buf_size - written) break;
    written += n;
  }
}

double calc_engine_get_main_number(CalcEngine *engine) {
  return prv_entry_to_double(engine);
}

void calc_engine_set_main_number(CalcEngine *engine, double val) {
  prv_double_to_entry(engine, val);
  if (engine->rpn_mode) {
    engine->stack[3] = val;
  }
}
