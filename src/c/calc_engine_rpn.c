#include "calc_engine.h"
#include "calc_engine_internal.h"

static void prv_stack_drop(CalcEngine *e) {
  // X ← Y, Y ← Z, Z ← T, T duplicates
  e->stack[3] = e->stack[2];
  e->stack[2] = e->stack[1];
  e->stack[1] = e->stack[0];
  // T stays
}

void ce_rpn_enter(CalcEngine *e) {
  double val = ce_entry_to_double(e);
  ce_stack_push(e, val);
  e->stack[3] = val; // X = same value (classic ENTER behavior)
  ce_double_to_entry(e, val);
  e->stack_lift_enabled = false; // next digit replaces X, no lift
}

void ce_rpn_operator(CalcEngine *e, CalcAction action) {
  if (e->error) return;

  // Finalize any in-progress entry into X
  double x = ce_entry_to_double(e);
  e->stack[3] = x;

  // Pop X and Y
  double x_val = e->stack[3];
  double y_val = e->stack[2];

  CalcOp op = ce_action_to_op(action);
  double result = ce_apply_op(y_val, op, x_val);

  if (ce_is_nan_or_inf(result) || ce_is_error(result)) {
    ce_set_error(e);
    return;
  }

  e->last_x = x_val;

  // Drop the stack (Y consumed), push result into X
  e->stack[2] = e->stack[1]; // Y ← Z
  e->stack[1] = e->stack[0]; // Z ← T
  // T stays (duplicates)
  e->stack[3] = result;      // X ← result

  ce_double_to_entry(e, result);
  e->stack_lift_enabled = true;
}

void ce_rpn_swap(CalcEngine *e) {
  if (e->entering) {
    e->stack[3] = ce_entry_to_double(e);
    e->entering = false;
  }
  double tmp = e->stack[3];
  e->stack[3] = e->stack[2];
  e->stack[2] = tmp;
  ce_double_to_entry(e, e->stack[3]);
  e->stack_lift_enabled = true;
}

// In RPN mode, BACKSPACE/CLEAR when there's no entry in progress clears X
// (HP-style "CLx") rather than dropping the stack — Y/Z/T are preserved.
void ce_rpn_clear_x(CalcEngine *e) {
  ce_clear_entry(e);
  e->stack[3] = 0.0;
  e->stack_lift_enabled = false;
}

void ce_rpn_handle_stack_op(CalcEngine *e, CalcAction action) {
  if (e->error) return;
  if (!e->rpn_mode) return;

  ce_terminate_entry(e);

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
        ce_stack_push(e, e->stack[3]);
      }
      e->stack[3] = e->last_x;
      break;
    default: break;
  }

  ce_double_to_entry(e, e->stack[3]);
  e->stack_lift_enabled = true;
}
