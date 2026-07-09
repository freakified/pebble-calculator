#pragma once

// Internal API shared between calc_engine.c (core) and its sibling files
// (calc_engine_std.c, calc_engine_rpn.c, calc_engine_sci.c). NOT for external
// use — never include from calc_engine.h, calculator.c, or any UI file.

#include "calc_engine.h"

// Domain-error sentinel returned by math helpers (e.g. divide by zero,
// sqrt of a negative). NaN never collides with a legitimate result, so the
// full double range stays available for large values like 10^20 or 170!.
#define ERROR_VALUE (__builtin_nan(""))

// ---------------------------------------------------------------------------
// Helpers defined in calc_engine.c (core)
// ---------------------------------------------------------------------------

bool   ce_is_error(double x);
bool   ce_is_nan_or_inf(double x);
void   ce_clear_entry(CalcEngine *e);
double ce_entry_to_double(CalcEngine *e);
void   ce_double_to_entry(CalcEngine *e, double val);
void   ce_set_error(CalcEngine *e);
void   ce_recover_from_error(CalcEngine *e);
double ce_apply_op(double left, CalcOp op, double right);
CalcOp ce_action_to_op(CalcAction action);
void   ce_stack_push(CalcEngine *e, double val);
// Commits any in-progress digit entry into X and enables stack-lift. Safe to
// call in standard mode (no-op when not entering).
void   ce_terminate_entry(CalcEngine *e);
const char *ce_op_to_str(CalcOp op);

// ---------------------------------------------------------------------------
// Module entry points (called by the core dispatcher)
// ---------------------------------------------------------------------------

// calc_engine_std.c
void ce_std_evaluate(CalcEngine *e);
void ce_std_operator(CalcEngine *e, CalcAction action);

// calc_engine_rpn.c
void ce_rpn_enter(CalcEngine *e);
void ce_rpn_operator(CalcEngine *e, CalcAction action);
void ce_rpn_swap(CalcEngine *e);
void ce_rpn_clear_x(CalcEngine *e);
void ce_rpn_handle_stack_op(CalcEngine *e, CalcAction action);

// calc_engine_sci.c
void ce_sci_handle_unary(CalcEngine *e, CalcAction action);
void ce_sci_handle_pi(CalcEngine *e);
void ce_sci_handle_e(CalcEngine *e);
