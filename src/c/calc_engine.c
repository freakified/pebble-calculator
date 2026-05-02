#include "calc_engine.h"
#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Internal helpers — no math.h, no strtod on Pebble
// ---------------------------------------------------------------------------

static double prv_fabs(double x) {
  return x < 0.0 ? -x : x;
}

// Simple error flag value — we use a very large magnitude as "error"
#define ERROR_VALUE 1e18
static bool prv_is_error(double x) {
  return prv_fabs(x) > 9.999e15;
}

// Count digit characters (0-9) in a string
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

// Parse the entry buffer to a double (hand-rolled, no strtod)
static double prv_entry_to_double(CalcEngine *e) {
  double result = 0.0;
  bool negative = false;
  int i = 0;

  if (e->entry[0] == '-') {
    negative = true;
    i = 1;
  }

  // Integer part
  for (; i < e->entry_len && e->entry[i] != '.'; i++) {
    result = result * 10.0 + (double)(e->entry[i] - '0');
  }

  // Fractional part
  if (i < e->entry_len && e->entry[i] == '.') {
    i++; // skip '.'
    double frac = 0.1;
    for (; i < e->entry_len; i++) {
      result += (double)(e->entry[i] - '0') * frac;
      frac *= 0.1;
    }
  }

  return negative ? -result : result;
}

// Format a double into scientific notation in the entry buffer.
// Target max length: CALC_SCI_MAX_CHARS.  Format: [-]D.DDDeDD
static void prv_format_scientific(CalcEngine *e, double val) {
  char buf[CALC_DISPLAY_MAX + 1];
  int pos = 0;
  bool negative = false;

  if (val < 0.0) {
    negative = true;
    val = -val;
  }

  // Find exponent: val = mantissa * 10^exp, 1.0 <= mantissa < 10.0
  int exp = 0;
  double mantissa = val;
  if (val >= 10.0) {
    while (mantissa >= 10.0) { mantissa /= 10.0; exp++; }
  } else if (val > 0.0 && val < 1.0) {
    while (mantissa < 1.0) { mantissa *= 10.0; exp--; }
  }

  if (negative) buf[pos++] = '-';

  // First mantissa digit
  int first = (int)mantissa;
  if (first > 9) first = 9;
  buf[pos++] = '0' + (char)first;
  mantissa -= (double)first;

  // Figure out how many chars the exponent part will take
  int abs_exp = exp < 0 ? -exp : exp;
  int exp_chars = 1; // 'e'
  if (exp < 0) exp_chars++; // '-'
  if (abs_exp >= 100) exp_chars += 3;
  else if (abs_exp >= 10) exp_chars += 2;
  else exp_chars += 1;

  // Available decimal mantissa digits
  int avail = CALC_SCI_MAX_CHARS - pos - exp_chars - 1; // -1 for '.'
  if (avail < 0) avail = 0;

  if (avail > 0 && mantissa > 0.0000001) {
    buf[pos++] = '.';
    for (int d = 0; d < avail; d++) {
      mantissa *= 10.0;
      int digit = (int)mantissa;
      if (digit > 9) digit = 9;
      buf[pos++] = '0' + (char)digit;
      mantissa -= (double)digit;
    }
    // Trim trailing zeros
    while (pos > 0 && buf[pos - 1] == '0') pos--;
    if (pos > 0 && buf[pos - 1] == '.') pos--;
  }

  // Append 'e' and exponent
  buf[pos++] = 'e';
  if (exp < 0) {
    buf[pos++] = '-';
    exp = -exp;
  }
  if (exp >= 100) {
    buf[pos++] = '0' + (char)(exp / 100);
    buf[pos++] = '0' + (char)((exp / 10) % 10);
    buf[pos++] = '0' + (char)(exp % 10);
  } else if (exp >= 10) {
    buf[pos++] = '0' + (char)(exp / 10);
    buf[pos++] = '0' + (char)(exp % 10);
  } else {
    buf[pos++] = '0' + (char)exp;
  }

  buf[pos] = '\0';
  memcpy(e->entry, buf, pos + 1);
  e->entry_len = pos;
  e->has_dot = (memchr(e->entry, '.', pos) != NULL);
  e->entering = false;
}

// Format a double into the entry buffer (hand-rolled, no %g).
// Uses scientific notation for values too large or too small to display
// as plain numbers within CALC_X_MAX_DIGITS digit characters.
static void prv_double_to_entry(CalcEngine *e, double val) {
  char buf[CALC_DISPLAY_MAX + 1];
  int pos = 0;
  bool negative = false;

  if (val < 0.0) {
    negative = true;
    val = -val;
  }

  // True error: beyond useful double range (e.g. div-by-zero overflow)
  if (val > 9.999e15) {
    snprintf(e->entry, sizeof(e->entry), "Error");
    e->entry_len = 5;
    e->has_dot = false;
    e->entering = false;
    e->error = true;
    return;
  }

  // How many digit slots do we have? (GOTHIC 28 threshold; LECO/GOTHIC
  // font selection is handled by the UI based on the resulting digit count)
  int max_digits = negative ? (CALC_X_MAX_DIGITS_GOTHIC - 1) : CALC_X_MAX_DIGITS_GOTHIC;

  // Count integer digits needed
  long long int_part = (long long)val;
  int int_digit_count = 0;
  {
    long long n = int_part;
    if (n == 0) {
      int_digit_count = 1;
    } else {
      while (n > 0) { int_digit_count++; n /= 10; }
    }
  }

  // Use scientific notation if integer part won't fit
  if (int_digit_count > max_digits) {
    prv_format_scientific(e, negative ? -val : val);
    return;
  }

  // Format integer part
  if (int_part == 0) {
    buf[pos++] = '0';
  } else {
    char tmp[16];
    int tmp_len = 0;
    long long n = int_part;
    while (n > 0 && tmp_len < 15) {
      tmp[tmp_len++] = '0' + (char)(n % 10);
      n /= 10;
    }
    for (int j = tmp_len - 1; j >= 0; j--) {
      buf[pos++] = tmp[j];
    }
  }

  // Format fractional part, capped so total digits <= max_digits
  double frac_part = val - (double)int_part;
  int digits_used = int_digit_count;
  int max_frac = max_digits - digits_used;

  if (frac_part > 0.0000000005 && max_frac > 0) {
    char frac_digits[10];
    int frac_len = 0;
    double f = frac_part;
    for (int d = 0; d < max_frac && d < 9; d++) {
      f *= 10.0;
      int digit = (int)f;
      if (digit > 9) digit = 9;
      frac_digits[frac_len++] = '0' + (char)digit;
      f -= (double)digit;
    }

    // Trim trailing zeros
    while (frac_len > 0 && frac_digits[frac_len - 1] == '0') {
      frac_len--;
    }

    if (frac_len > 0) {
      buf[pos++] = '.';
      for (int j = 0; j < frac_len; j++) {
        buf[pos++] = frac_digits[j];
      }
    }
  }

  // Check if a small nonzero value rounded to "0" — use sci notation instead
  if (val > 0.0 && pos == 1 && buf[0] == '0') {
    prv_format_scientific(e, negative ? -val : val);
    return;
  }

  buf[pos] = '\0';

  // Prepend negative sign
  if (negative && !(pos == 1 && buf[0] == '0')) {
    if (pos + 1 < CALC_DISPLAY_MAX) {
      for (int j = pos; j >= 0; j--) {
        buf[j + 1] = buf[j];
      }
      buf[0] = '-';
      pos++;
    }
  }

  memcpy(e->entry, buf, pos + 1);
  e->entry_len = pos;
  e->has_dot = (memchr(e->entry, '.', pos) != NULL);
  e->entering = false;
}

// Format a double into an external buffer
static void prv_format_double(double val, char *buf, int buf_size) {
  // Reuse the entry formatter via a temporary engine
  CalcEngine tmp;
  memset(&tmp, 0, sizeof(tmp));
  prv_double_to_entry(&tmp, val);
  int len = tmp.entry_len;
  if (len >= buf_size) len = buf_size - 1;
  memcpy(buf, tmp.entry, len);
  buf[len] = '\0';
}

static void prv_set_error(CalcEngine *e) {
  e->error = true;
  snprintf(e->entry, sizeof(e->entry), "Error");
  e->entry_len = 5;
  e->entering = false;
}

static double prv_apply_op(double left, CalcOp op, double right) {
  switch (op) {
    case CALC_OP_ADD:      return left + right;
    case CALC_OP_SUBTRACT: return left - right;
    case CALC_OP_MULTIPLY: return left * right;
    case CALC_OP_DIVIDE:   return right != 0.0 ? left / right : ERROR_VALUE;
    default:               return right;
  }
}

static CalcOp prv_action_to_op(CalcAction action) {
  switch (action) {
    case CALC_ACTION_ADD:      return CALC_OP_ADD;
    case CALC_ACTION_SUBTRACT: return CALC_OP_SUBTRACT;
    case CALC_ACTION_MULTIPLY: return CALC_OP_MULTIPLY;
    case CALC_ACTION_DIVIDE:   return CALC_OP_DIVIDE;
    default:                   return CALC_OP_NONE;
  }
}

// RPN stack helpers
static void prv_stack_push(CalcEngine *e, double val) {
  // Shift up: T is lost, Z←Y, Y←X, X←val
  e->stack[0] = e->stack[1]; // T ← Z
  e->stack[1] = e->stack[2]; // Z ← Y
  e->stack[2] = e->stack[3]; // Y ← X
  e->stack[3] = val;         // X ← val
}

// ---------------------------------------------------------------------------
// Digit / dot entry
// ---------------------------------------------------------------------------

static void prv_handle_digit(CalcEngine *e, int digit) {
  if (e->error) {
    e->error = false;
    if (!e->rpn_mode) {
      bool rpn = e->rpn_mode;
      calc_engine_init(e);
      e->rpn_mode = rpn;
    } else {
      e->entry[0] = '0';
      e->entry[1] = '\0';
      e->entry_len = 1;
      e->has_dot = false;
      e->entering = false;
    }
  }

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
  int max_digits = (e->entry[0] == '-') ? (CALC_X_MAX_DIGITS_GOTHIC - 1) : CALC_X_MAX_DIGITS_GOTHIC;
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
  if (e->error) {
    e->error = false;
    if (!e->rpn_mode) {
      bool rpn = e->rpn_mode;
      calc_engine_init(e);
      e->rpn_mode = rpn;
    } else {
      e->entry[0] = '0';
      e->entry[1] = '\0';
      e->entry_len = 1;
      e->has_dot = false;
      e->entering = false;
    }
  }

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

static void prv_standard_evaluate(CalcEngine *e) {
  if (e->pending_op == CALC_OP_NONE) return;

  double right = prv_entry_to_double(e);
  double result = prv_apply_op(e->pending_value, e->pending_op, right);

  if (prv_is_error(result)) {
    prv_set_error(e);
    e->pending_op = CALC_OP_NONE;
    return;
  }

  prv_double_to_entry(e, result);
  e->pending_op = CALC_OP_NONE;
  e->pending_value = 0;
}

static void prv_standard_operator(CalcEngine *e, CalcAction action) {
  if (e->error) return;

  // If there's a pending operation, evaluate it first (chaining)
  if (e->pending_op != CALC_OP_NONE && e->entering) {
    double right = prv_entry_to_double(e);
    double result = prv_apply_op(e->pending_value, e->pending_op, right);
    if (prv_is_error(result)) {
      prv_set_error(e);
      e->pending_op = CALC_OP_NONE;
      return;
    }
    prv_double_to_entry(e, result);
  }

  e->pending_value = prv_entry_to_double(e);
  e->pending_op = prv_action_to_op(action);
  e->entering = false;
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

  if (prv_is_error(result)) {
    prv_set_error(e);
    return;
  }

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

static void prv_rpn_drop(CalcEngine *e) {
  // Drop X, shift down
  e->stack[3] = e->stack[2];
  e->stack[2] = e->stack[1];
  e->stack[1] = e->stack[0];
  // T stays
  prv_double_to_entry(e, e->stack[3]);
  e->stack_lift_enabled = true;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void calc_engine_init(CalcEngine *engine) {
  memset(engine, 0, sizeof(CalcEngine));
  prv_clear_entry(engine);
  engine->pending_op = CALC_OP_NONE;
  engine->stack_lift_enabled = false;
}

void calc_engine_set_rpn_mode(CalcEngine *engine, bool rpn) {
  engine->rpn_mode = rpn;
  calc_engine_init(engine);
  engine->rpn_mode = rpn; // restore after init clears it
}

void calc_engine_handle_action(CalcEngine *engine, CalcAction action) {
  // Clear error on any clear action
  if (action == CALC_ACTION_CLEAR) {
    bool rpn = engine->rpn_mode;
    calc_engine_init(engine);
    engine->rpn_mode = rpn;
    return;
  }

  if (action == CALC_ACTION_BACKSPACE) {
    if (engine->error) {
      engine->error = false;
      if (!engine->rpn_mode) {
        bool rpn = engine->rpn_mode;
        calc_engine_init(engine);
        engine->rpn_mode = rpn;
      } else {
        prv_clear_entry(engine);
        engine->stack[3] = 0.0;
      }
      return;
    }
    // Backspace only edits an in-progress entry; results are untouched
    // (long-press CLEAR/DROP handles the result case).
    if (!engine->entering) return;

    if (engine->entry_len <= 1) {
      engine->entry[0] = '0';
      engine->entry[1] = '\0';
      engine->entry_len = 1;
      engine->has_dot = false;
      if (engine->rpn_mode) engine->stack[3] = 0.0;
      return;
    }
    if (engine->entry[engine->entry_len - 1] == '.') {
      engine->has_dot = false;
    }
    engine->entry_len--;
    engine->entry[engine->entry_len] = '\0';
    // Don't leave just "-" sitting in the buffer.
    if (engine->entry_len == 1 && engine->entry[0] == '-') {
      engine->entry[0] = '0';
      engine->entry[1] = '\0';
      engine->entry_len = 1;
      engine->has_dot = false;
      if (engine->rpn_mode) engine->stack[3] = 0.0;
      return;
    }
    if (engine->rpn_mode) {
      engine->stack[3] = prv_entry_to_double(engine);
    }
    return;
  }

  // Digits
  if (action <= CALC_ACTION_DIGIT_9) {
    prv_handle_digit(engine, (int)action - (int)CALC_ACTION_DIGIT_0);
    if (engine->rpn_mode) {
      engine->stack[3] = prv_entry_to_double(engine);
    }
    return;
  }

  // Decimal point
  if (action == CALC_ACTION_DOT) {
    prv_handle_dot(engine);
    return;
  }

  // Negate
  if (action == CALC_ACTION_NEGATE) {
    if (engine->error) return;
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
    return;
  }

  // Operators
  if (action == CALC_ACTION_ADD || action == CALC_ACTION_SUBTRACT ||
      action == CALC_ACTION_MULTIPLY || action == CALC_ACTION_DIVIDE) {
    if (engine->rpn_mode) {
      prv_rpn_operator(engine, action);
    } else {
      prv_standard_operator(engine, action);
    }
    return;
  }

  // Equals / Enter
  if (action == CALC_ACTION_EQUALS) {
    if (engine->rpn_mode) {
      prv_rpn_enter(engine);
    } else {
      prv_standard_evaluate(engine);
    }
    return;
  }

  if (action == CALC_ACTION_ENTER) {
    prv_rpn_enter(engine);
    return;
  }

  // RPN-specific
  if (action == CALC_ACTION_SWAP) {
    if (engine->rpn_mode) prv_rpn_swap(engine);
    return;
  }

  if (action == CALC_ACTION_DROP) {
    if (engine->error) engine->error = false;
    if (engine->rpn_mode) prv_rpn_drop(engine);
    return;
  }
}

const char *calc_engine_get_x_display(CalcEngine *engine) {
  return engine->entry;
}

void calc_engine_get_stack_display(CalcEngine *engine, int reg, char *buf, int buf_size) {
  if (reg < 0 || reg > 2) {
    buf[0] = '\0';
    return;
  }
  double val = engine->stack[reg]; // 0=T, 1=Z, 2=Y
  prv_format_double(val, buf, buf_size);
}

void calc_engine_get_secondary_display(CalcEngine *engine, char *buf, int buf_size) {
  if (engine->rpn_mode) {
    calc_engine_get_stack_display(engine, 2, buf, buf_size);
    return;
  }

  // Standard mode: show pending operand and operator
  if (engine->pending_op == CALC_OP_NONE) {
    buf[0] = '\0';
    return;
  }

  const char *op_str = "";
  switch (engine->pending_op) {
    case CALC_OP_ADD:      op_str = "+"; break;
    case CALC_OP_SUBTRACT: op_str = "-"; break;
    case CALC_OP_MULTIPLY: op_str = "x"; break;
    case CALC_OP_DIVIDE:   op_str = "/"; break;
    default: break;
  }

  char val_buf[CALC_DISPLAY_MAX + 1];
  prv_format_double(engine->pending_value, val_buf, sizeof(val_buf));
  snprintf(buf, buf_size, "%s %s", val_buf, op_str);
}
