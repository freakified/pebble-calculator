#include "calc_engine.h"
#include "calc_engine_internal.h"
#include "calc_format.h"
#include "calc_math.h"
#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static double prv_fabs(double x) {
  return x < 0.0 ? -x : x;
}

bool ce_is_error(double x) {
  return !(x == x); // NaN sentinel (ERROR_VALUE)
}

// NaN/Infinity detection without relying on isnan/isinf macros — anything
// outside the doubles we can format is treated as an error.
bool ce_is_nan_or_inf(double x) {
  return !(x == x) || prv_fabs(x) > CALC_FORMAT_MAX_ABS;
}

static int prv_count_digits(const char *s, int len) {
  int count = 0;
  for (int i = 0; i < len; i++) {
    if (s[i] >= '0' && s[i] <= '9') count++;
  }
  return count;
}

void ce_clear_entry(CalcEngine *e) {
  e->entry[0] = '0';
  e->entry[1] = '\0';
  e->entry_len = 1;
  e->has_dot = false;
  e->entering = false;
}

double ce_entry_to_double(CalcEngine *e) {
  if (e->error || e->entry[0] == 'E') return 0.0;
  return calc_format_parse(e->entry, e->entry_len);
}

void ce_double_to_entry(CalcEngine *e, double val) {
  bool overflow = false;
  e->entry_len = calc_format_double(val, e->entry, &overflow);
  e->has_dot = (memchr(e->entry, '.', e->entry_len) != NULL);
  e->entering = false;
  if (overflow) e->error = true;
}

void ce_set_error(CalcEngine *e) {
  e->error = true;
  snprintf(e->entry, sizeof(e->entry), "Error");
  e->entry_len = 5;
  e->entering = false;
}

// Recover from error state in response to a digit/dot keypress: standard mode
// fully re-initializes; RPN keeps the stack so the user can resume from the
// pre-error operands (T/Z/Y intact, X gets the new entry).
void ce_recover_from_error(CalcEngine *e) {
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
    ce_clear_entry(e);
  }
}

double ce_apply_op(double left, CalcOp op, double right) {
  switch (op) {
    case CALC_OP_ADD:      return left + right;
    case CALC_OP_SUBTRACT: return left - right;
    case CALC_OP_MULTIPLY: return left * right;
    case CALC_OP_DIVIDE:   return right != 0.0 ? left / right : ERROR_VALUE;
    case CALC_OP_POWER:    return calc_math_pow(left, right);
    case CALC_OP_NTHROOT:  return right != 0.0 ? calc_math_pow(left, 1.0 / right) : ERROR_VALUE;
    default:               return right;
  }
}

CalcOp ce_action_to_op(CalcAction action) {
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

const char *ce_op_to_str(CalcOp op) {
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

// RPN stack helpers
void ce_stack_push(CalcEngine *e, double val) {
  // Shift up: T is lost, Z←Y, Y←X, X←val
  e->stack[0] = e->stack[1]; // T ← Z
  e->stack[1] = e->stack[2]; // Z ← Y
  e->stack[2] = e->stack[3]; // Y ← X
  e->stack[3] = val;         // X ← val
}

// Commit any in-progress digit entry into X, and treat that just-entered value
// as a result for stack-lift purposes (next push will lift). Safe to call in
// standard mode (no-op when not entering).
void ce_terminate_entry(CalcEngine *e) {
  if (e->entering) {
    e->stack[3] = ce_entry_to_double(e);
    e->entering = false;
    e->stack_lift_enabled = true;
  }
}

// ---------------------------------------------------------------------------
// Digit / dot entry
// ---------------------------------------------------------------------------

static void prv_handle_digit(CalcEngine *e, int digit) {
  if (e->error) ce_recover_from_error(e);

  // A typed digit always starts a fresh right operand, replacing any committed
  // result (unary/constant/MR) sitting in entry.
  e->right_committed = false;

  if (e->rpn_mode && !e->entering && e->stack_lift_enabled) {
    // Lift the stack before starting new entry
    ce_stack_push(e, ce_entry_to_double(e));
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

  // Exponent entry (after EE): digits go to the exponent, capped at 3, and
  // skip the mantissa rules below.
  char *epos = memchr(e->entry, 'e', e->entry_len);
  if (epos) {
    int exp_start = (int)(epos - e->entry) + 1;
    if (exp_start < e->entry_len && e->entry[exp_start] == '-') exp_start++;
    int exp_digits = e->entry_len - exp_start;
    if (exp_digits == 1 && e->entry[exp_start] == '0') {
      e->entry[exp_start] = '0' + digit; // replace a leading exponent zero
      return;
    }
    if (exp_digits >= 3 || e->entry_len >= CALC_DISPLAY_MAX) return;
    if (calc_format_display_width(e->entry, e->entry_len) + CALC_FORMAT_W_DIGIT >
        CALC_FORMAT_DISPLAY_W) return;
    e->entry[e->entry_len++] = '0' + digit;
    e->entry[e->entry_len] = '\0';
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
  if (e->error) ce_recover_from_error(e);

  e->right_committed = false;

  if (e->rpn_mode && !e->entering && e->stack_lift_enabled) {
    ce_stack_push(e, ce_entry_to_double(e));
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
  if (memchr(e->entry, 'e', e->entry_len)) return; // no dot in an exponent
  if (e->entry_len >= CALC_DISPLAY_MAX - 1) return; // buffer safety cap

  e->entry[e->entry_len] = '.';
  e->entry_len++;
  e->entry[e->entry_len] = '\0';
  e->has_dot = true;
}

// ---------------------------------------------------------------------------
// Memory operations
// ---------------------------------------------------------------------------

static void prv_handle_memory(CalcEngine *e, CalcAction action) {
  if (e->error) return;

  // Read current X value (committing any in-progress entry).
  double x;
  if (e->rpn_mode) {
    ce_terminate_entry(e);
    x = e->stack[3];
  } else {
    x = ce_entry_to_double(e);
  }

  switch (action) {
    case CALC_ACTION_M_PLUS:    e->memory += x; break;
    case CALC_ACTION_M_MINUS:   e->memory -= x; break;
    case CALC_ACTION_M_CLEAR:   e->memory = 0.0; break;
    case CALC_ACTION_M_RECALL:
      // Recall pushes memory onto X with stack lift — typed values that were
      // just terminated are preserved on Y (ce_terminate_entry sets lift=true).
      if (e->rpn_mode) {
        if (e->stack_lift_enabled) {
          ce_stack_push(e, e->stack[3]);
        }
        e->stack[3] = e->memory;
        e->stack_lift_enabled = true;
      }
      ce_double_to_entry(e, e->memory);
      if (e->rpn_mode) e->stack[3] = e->memory;
      else e->right_committed = true;
      break;
    default: break;
  }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void calc_engine_init(CalcEngine *engine) {
  memset(engine, 0, sizeof(CalcEngine));
  ce_clear_entry(engine);
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

  // DEG⇄RAD is a mode flip, not a computation — like paging, it leaves the
  // entry, tape, and INV state untouched.
  if (action == CALC_ACTION_DRG_TOGGLE) {
    engine->deg_mode = !engine->deg_mode;
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

  // Reset just_cleared on any action outside the Clear/Backspace family. (Those
  // two actions manage the flag themselves, below, to implement the C→AC
  // double-tap — resetting it unconditionally here would wipe it before the
  // Clear/Backspace handler ever got to read it.)
  if (resolved != CALC_ACTION_CLEAR && resolved != CALC_ACTION_BACKSPACE) {
    engine->just_cleared = false;
  }

  // Backspace / Clear
  if (resolved == CALC_ACTION_BACKSPACE || resolved == CALC_ACTION_CLEAR) {
    if (engine->error) {
      if (engine->rpn_mode) {
        engine->error = false;
        ce_rpn_clear_x(engine);
      } else {
        ce_recover_from_error(engine);
      }
      goto done;
    }

    if (engine->entering) {
      if (engine->entry_len <= 1) {
        ce_clear_entry(engine);
        if (engine->rpn_mode) engine->stack[3] = 0.0;
        goto done;
      }
      if (engine->entry[engine->entry_len - 1] == '.') {
        engine->has_dot = false;
      }
      engine->entry_len--;
      engine->entry[engine->entry_len] = '\0';
      if (engine->entry_len == 1 && engine->entry[0] == '-') {
        ce_clear_entry(engine);
        if (engine->rpn_mode) engine->stack[3] = 0.0;
        goto done;
      }
      if (engine->rpn_mode) {
        engine->stack[3] = ce_entry_to_double(engine);
      }
      goto done;
    }

    // Not entering, no error
    if (engine->rpn_mode) {
      if (engine->just_cleared) {
        // AC: clear full stack and memory, mirroring standard mode's
        // C→AC (which wipes the memory register on the second press).
        for (int i = 0; i < 4; i++) engine->stack[i] = 0.0;
        engine->memory = 0.0;
        engine->last_x = 0.0;
        ce_clear_entry(engine);
        engine->just_cleared = false;
      } else {
        ce_rpn_clear_x(engine);
        engine->just_cleared = true;
      }
    } else if (resolved == CALC_ACTION_CLEAR) {
      int  page = engine->page;
      bool deg  = engine->deg_mode;
      if (engine->just_cleared) {
        // AC: clear everything including memory
        calc_engine_init(engine);
        engine->page = page;
        engine->deg_mode = deg;
      } else {
        // C: preserve memory
        double mem = engine->memory;
        calc_engine_init(engine);
        engine->page = page;
        engine->deg_mode = deg;
        engine->memory = mem;
        engine->just_cleared = true;
      }
    }
    goto done;
  }

  // Digits
  if (resolved <= CALC_ACTION_DIGIT_9) {
    prv_handle_digit(engine, (int)resolved - (int)CALC_ACTION_DIGIT_0);
    if (engine->rpn_mode) {
      engine->stack[3] = ce_entry_to_double(engine);
    }
    goto done;
  }

  // Decimal point
  if (resolved == CALC_ACTION_DOT) {
    prv_handle_dot(engine);
    goto done;
  }

  // EE — begin exponent entry. On a fresh entry seeds "1e" (so EE 5 = means
  // 1e5); while typing appends 'e' to the mantissa.
  if (resolved == CALC_ACTION_EE) {
    if (engine->error) ce_recover_from_error(engine);
    engine->right_committed = false;
    if (!engine->entering) {
      if (engine->rpn_mode && engine->stack_lift_enabled) {
        ce_stack_push(engine, ce_entry_to_double(engine));
      }
      engine->entry[0] = '1';
      engine->entry[1] = 'e';
      engine->entry[2] = '\0';
      engine->entry_len = 2;
      engine->has_dot = false;
      engine->entering = true;
    } else if (memchr(engine->entry, 'e', engine->entry_len) == NULL &&
               engine->entry_len < CALC_DISPLAY_MAX &&
               // Room for the 'e' plus at least one exponent digit — a
               // mantissa typed to the display's full width can't take an
               // exponent, so the keypress is ignored like any other
               // full-display input.
               calc_format_display_width(engine->entry, engine->entry_len) +
                       2 * CALC_FORMAT_W_DIGIT <= CALC_FORMAT_DISPLAY_W) {
      engine->entry[engine->entry_len++] = 'e';
      engine->entry[engine->entry_len] = '\0';
    }
    if (engine->rpn_mode) {
      engine->stack[3] = ce_entry_to_double(engine);
    }
    goto done;
  }

  // Negate
  if (resolved == CALC_ACTION_NEGATE) {
    if (engine->error) goto done;
    if (engine->entering) {
      // During exponent entry, ± flips the exponent's sign (calculator
      // convention), not the mantissa's.
      char *epos = memchr(engine->entry, 'e', engine->entry_len);
      if (epos) {
        int at = (int)(epos - engine->entry) + 1;
        if (at < engine->entry_len && engine->entry[at] == '-') {
          memmove(engine->entry + at, engine->entry + at + 1,
                  engine->entry_len - at); // includes the null
          engine->entry_len--;
        } else if (engine->entry_len < CALC_DISPLAY_MAX &&
                   calc_format_display_width(engine->entry, engine->entry_len) +
                           CALC_FORMAT_W_MINUS <= CALC_FORMAT_DISPLAY_W) {
          memmove(engine->entry + at + 1, engine->entry + at,
                  engine->entry_len - at + 1);
          engine->entry[at] = '-';
          engine->entry_len++;
        }
      } else if (engine->entry[0] == '-') {
        memmove(engine->entry, engine->entry + 1, engine->entry_len);
        engine->entry_len--;
      } else {
        if (engine->entry_len < CALC_DISPLAY_MAX - 1 &&
            calc_format_display_width(engine->entry, engine->entry_len) +
                    CALC_FORMAT_W_MINUS <= CALC_FORMAT_DISPLAY_W) {
          memmove(engine->entry + 1, engine->entry, engine->entry_len + 1);
          engine->entry[0] = '-';
          engine->entry_len++;
        }
      }
    } else {
      double val = ce_entry_to_double(engine);
      val = -val;
      ce_double_to_entry(engine, val);
    }
    if (engine->rpn_mode) {
      engine->stack[3] = ce_entry_to_double(engine);
    }
    goto done;
  }

  // Binary operators (basic + power/nth-root)
  if (resolved == CALC_ACTION_ADD || resolved == CALC_ACTION_SUBTRACT ||
      resolved == CALC_ACTION_MULTIPLY || resolved == CALC_ACTION_DIVIDE ||
      resolved == CALC_ACTION_POW || resolved == CALC_ACTION_NTHROOT) {
    if (engine->rpn_mode) {
      ce_rpn_operator(engine, resolved);
    } else {
      ce_std_operator(engine, resolved);
    }
    goto done;
  }

  // Equals / Enter
  if (resolved == CALC_ACTION_EQUALS) {
    if (engine->rpn_mode) {
      ce_rpn_enter(engine);
    } else {
      ce_std_evaluate(engine);
    }
    goto done;
  }

  if (resolved == CALC_ACTION_ENTER) {
    ce_rpn_enter(engine);
    goto done;
  }

  // RPN swap
  if (resolved == CALC_ACTION_SWAP) {
    if (engine->rpn_mode) ce_rpn_swap(engine);
    goto done;
  }

  // Unary scientific
  if (resolved == CALC_ACTION_SIN || resolved == CALC_ACTION_COS ||
      resolved == CALC_ACTION_TAN || resolved == CALC_ACTION_ASIN ||
      resolved == CALC_ACTION_ACOS || resolved == CALC_ACTION_ATAN ||
      resolved == CALC_ACTION_LN || resolved == CALC_ACTION_LOG10 ||
      resolved == CALC_ACTION_EXP || resolved == CALC_ACTION_POW10 ||
      resolved == CALC_ACTION_SQRT || resolved == CALC_ACTION_SQUARE ||
      resolved == CALC_ACTION_TO_HMS || resolved == CALC_ACTION_TO_H ||
      resolved == CALC_ACTION_RECIP || resolved == CALC_ACTION_FACT) {
    ce_sci_handle_unary(engine, resolved);
    goto done;
  }

  // Percent. Standard mode follows desk-calculator convention: with a pending
  // + or − the entry becomes that percentage OF the left operand ("200 + 10%"
  // → 200 + 20); with ×/÷ or no pending op it's a bare factor (x/100). RPN is
  // the HP behavior: X ← Y·X/100 with Y preserved.
  if (resolved == CALC_ACTION_PERCENT) {
    if (engine->error) goto done;
    if (engine->rpn_mode) {
      ce_terminate_entry(engine);
      double x = engine->stack[3];
      double result = engine->stack[2] * x / 100.0;
      if (ce_is_nan_or_inf(result)) { ce_set_error(engine); goto done; }
      engine->last_x = x;
      engine->stack[3] = result;
      ce_double_to_entry(engine, result);
      engine->stack_lift_enabled = true;
    } else {
      double x = ce_entry_to_double(engine);
      double result = x / 100.0;
      if (engine->op_stack_size > 0) {
        CalcOpFrame *top = &engine->op_stack[engine->op_stack_size - 1];
        if (top->op == CALC_OP_ADD || top->op == CALC_OP_SUBTRACT) {
          result = top->value * x / 100.0;
        }
      }
      if (ce_is_nan_or_inf(result)) { ce_set_error(engine); goto done; }
      ce_double_to_entry(engine, result);
      engine->right_committed = true;
    }
    goto done;
  }

  // Constants
  if (resolved == CALC_ACTION_PI) {
    ce_sci_handle_pi(engine);
    goto done;
  }
  if (resolved == CALC_ACTION_E) {
    ce_sci_handle_e(engine);
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
    ce_rpn_handle_stack_op(engine, resolved);
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
                     val_buf, ce_op_to_str(f.op));
    if (n < 0 || n >= buf_size - written) break;
    written += n;
  }
}

double calc_engine_get_main_number(CalcEngine *engine) {
  return ce_entry_to_double(engine);
}

void calc_engine_set_main_number(CalcEngine *engine, double val) {
  ce_double_to_entry(engine, val);
  if (engine->rpn_mode) {
    engine->stack[3] = val;
  }
}
