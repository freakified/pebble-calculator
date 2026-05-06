#include "calc_engine.h"
#include "calc_engine_internal.h"
#include "calc_format.h"
#include <stdio.h>

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

// Pop the top frame and apply its op to the current entry, writing the result
// back into entry. Returns false (and sets engine error) on math failure.
static bool prv_op_stack_fold_top(CalcEngine *e) {
  CalcOpFrame top = e->op_stack[e->op_stack_size - 1];
  e->op_stack_size--;

  double right = ce_entry_to_double(e);
  double result = ce_apply_op(top.value, top.op, right);

  if (ce_is_nan_or_inf(result) || ce_is_error(result)) {
    ce_set_error(e);
    return false;
  }
  ce_double_to_entry(e, result);
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
                     val_buf, ce_op_to_str(f.op));
    if (n < 0 || n >= buf_size - written) { buf[0] = '\0'; return; }
    written += n;
  }
  char r_buf[CALC_FORMAT_BUF_SIZE];
  calc_format_double(ce_entry_to_double(e), r_buf, NULL);
  int n = snprintf(buf + written, buf_size - written, "%s =", r_buf);
  if (n < 0 || n >= buf_size - written) buf[0] = '\0';
}

void ce_std_evaluate(CalcEngine *e) {
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

void ce_std_operator(CalcEngine *e, CalcAction action) {
  if (e->error) return;

  CalcOp new_op = ce_action_to_op(action);

  // No operand was typed since the last op press (or =), and entry doesn't
  // hold a committed result either — the user is just changing their mind
  // about which operator to apply. Replace the top frame's op (or push a fresh
  // frame if the stack is empty).
  if (!e->entering && !e->right_committed) {
    if (e->op_stack_size > 0) {
      e->op_stack[e->op_stack_size - 1].op = new_op;
    } else {
      e->op_stack[0].value = ce_entry_to_double(e);
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

  if (e->op_stack_size == CALC_OP_STACK_DEPTH) { ce_set_error(e); return; }
  e->op_stack[e->op_stack_size].value = ce_entry_to_double(e);
  e->op_stack[e->op_stack_size].op    = new_op;
  e->op_stack_size++;
  e->entering = false;
  e->right_committed = false;
}
