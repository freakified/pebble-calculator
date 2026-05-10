#include "calc_engine.h"
#include "calc_format.h"
#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Internal helpers — no math.h, no strtod on Pebble
// ---------------------------------------------------------------------------

static double prv_fabs(double x) {
  return x < 0.0 ? -x : x;
}

#define ERROR_VALUE 1e18
static bool prv_is_error(double x) {
  return prv_fabs(x) > 9.999e15;
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
    calc_engine_init(e);
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
  if (e->error) prv_recover_from_error(e);

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

// In RPN mode, BACKSPACE/CLEAR when there's no entry in progress clears X
// (HP-style "CLx") rather than dropping the stack — Y/Z/T are preserved.
static void prv_rpn_clear_x(CalcEngine *e) {
  prv_clear_entry(e);
  e->stack[3] = 0.0;
  e->stack_lift_enabled = false;
}

static bool prv_clear_error(CalcEngine *e) {
  if (!e->error) return false;
  if (e->rpn_mode) {
    e->error = false;
    prv_rpn_clear_x(e);
  } else {
    calc_engine_init(e);
  }
  return true;
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
  calc_engine_init(engine);
  engine->rpn_mode = rpn;
}

void calc_engine_handle_action(CalcEngine *engine, CalcAction action) {
  // Clear all (or clear-X if RPN)
  if (action == CALC_ACTION_CLEAR_ALL) {
    if (prv_clear_error(engine)) {
      return;
    }

    if (engine->rpn_mode) {
      prv_rpn_clear_x(engine);
      return;
    }

    calc_engine_init(engine);
    return;
  }

  // Clear entry (or clear-X if RPN)
  if (action == CALC_ACTION_CLEAR_ENTRY) {
    if (prv_clear_error(engine)) {
      return;
    }

    if (engine->rpn_mode) {
      prv_rpn_clear_x(engine);
      return;
    }

    prv_clear_entry(engine);
    return;
  }

  // Backspace / Clear
  if (action == CALC_ACTION_BACKSPACE) {
    if (prv_clear_error(engine)) {
      return;
    }

    if (engine->entering) {
      if (engine->entry_len <= 1) {
        prv_clear_entry(engine);
        if (engine->rpn_mode) engine->stack[3] = 0.0;
        return;
      }
      if (engine->entry[engine->entry_len - 1] == '.') {
        engine->has_dot = false;
      }
      engine->entry_len--;
      engine->entry[engine->entry_len] = '\0';
      if (engine->entry_len == 1 && engine->entry[0] == '-') {
        prv_clear_entry(engine);
        if (engine->rpn_mode) engine->stack[3] = 0.0;
        return;
      }
      if (engine->rpn_mode) {
        engine->stack[3] = prv_entry_to_double(engine);
      }
      return;
    }

    // Not entering, no error
    if (engine->rpn_mode) {
      prv_rpn_clear_x(engine);
    } else {
      prv_clear_entry(engine);
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

  char val_buf[CALC_FORMAT_BUF_SIZE];
  calc_format_double(engine->pending_value, val_buf, NULL);
  snprintf(buf, buf_size, "%s %s", val_buf, op_str);
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
