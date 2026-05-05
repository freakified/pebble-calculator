#pragma once

#include <pebble.h>
#include "calc_format.h"

// Maximum characters in the display buffer (must hold 13 digits + sign + dot + null)
#define CALC_DISPLAY_MAX 16

// Max digit characters (0-9) that fit in the X display with LECO 32 Bold.
// The decimal point is narrower and doesn't consume a digit slot.
// The minus sign DOES consume one digit slot.
#define CALC_X_MAX_DIGITS_LECO 7

// Number of pages cycled through by the SELECT button.
#define CALC_PAGE_COUNT 3
#define CALC_PAGE_BASIC 0
#define CALC_PAGE_SCI 1
#define CALC_PAGE_MEM 2

// Actions that buttons can trigger
typedef enum {
  CALC_ACTION_DIGIT_0 = 0,
  CALC_ACTION_DIGIT_1,
  CALC_ACTION_DIGIT_2,
  CALC_ACTION_DIGIT_3,
  CALC_ACTION_DIGIT_4,
  CALC_ACTION_DIGIT_5,
  CALC_ACTION_DIGIT_6,
  CALC_ACTION_DIGIT_7,
  CALC_ACTION_DIGIT_8,
  CALC_ACTION_DIGIT_9,
  CALC_ACTION_DOT,
  CALC_ACTION_ADD,
  CALC_ACTION_SUBTRACT,
  CALC_ACTION_MULTIPLY,
  CALC_ACTION_DIVIDE,
  CALC_ACTION_NEGATE,
  CALC_ACTION_EQUALS,    // Standard mode: evaluate
  CALC_ACTION_CLEAR,     // C (or clear-X in RPN mode)
  CALC_ACTION_BACKSPACE, // Remove last char from current entry
  // RPN-specific
  CALC_ACTION_ENTER,     // Push X onto stack
  CALC_ACTION_SWAP,      // Swap X <-> Y

  // Scientific (unary)
  CALC_ACTION_SIN,
  CALC_ACTION_COS,
  CALC_ACTION_TAN,
  CALC_ACTION_ASIN,
  CALC_ACTION_ACOS,
  CALC_ACTION_ATAN,
  CALC_ACTION_LN,
  CALC_ACTION_LOG10,
  CALC_ACTION_EXP,       // e^x
  CALC_ACTION_POW10,     // 10^x
  CALC_ACTION_SQRT,
  CALC_ACTION_SQUARE,
  CALC_ACTION_CUBE,
  CALC_ACTION_CBRT,
  CALC_ACTION_RECIP,
  CALC_ACTION_FACT,

  // Scientific (binary — chainable via the standard pending-op path or RPN-pop path)
  CALC_ACTION_POW,       // y^x
  CALC_ACTION_NTHROOT,   // x-th root of y

  // Constants
  CALC_ACTION_PI,
  CALC_ACTION_E,

  // Modifier
  CALC_ACTION_2ND_TOGGLE,

  // Memory
  CALC_ACTION_M_PLUS,
  CALC_ACTION_M_MINUS,
  CALC_ACTION_M_RECALL,
  CALC_ACTION_M_CLEAR,

  // RPN stack ops
  CALC_ACTION_ROLL_DOWN,
  CALC_ACTION_ROLL_UP,
  CALC_ACTION_DROP,
  CALC_ACTION_STACK_CLEAR,
  CALC_ACTION_LAST_X,

  // Paging
  CALC_ACTION_PAGE_NEXT,

  // Sentinel for empty button slots
  CALC_ACTION_NOOP,
} CalcAction;

// Pending operator for standard mode
typedef enum {
  CALC_OP_NONE = 0,
  CALC_OP_ADD,
  CALC_OP_SUBTRACT,
  CALC_OP_MULTIPLY,
  CALC_OP_DIVIDE,
  CALC_OP_POWER,
  CALC_OP_NTHROOT,
} CalcOp;

// Max depth of the standard-mode operator-precedence stack. Right-associative
// power chains are the worst case; 8 leaves comfortable headroom on a watch UI.
#define CALC_OP_STACK_DEPTH 8

// Worst-case secondary-display buffer: every op-stack frame rendered as
// "value op " plus the post-'=' tape view, which adds one more term (right
// operand) and " =". The renderer right-aligns and ellipsis-clips, so the
// full string must fit here for the oldest frames to elide correctly.
#define CALC_SECONDARY_BUF_SIZE ((CALC_OP_STACK_DEPTH + 1) * (CALC_FORMAT_BUF_SIZE + 4))

typedef struct {
  double value;
  CalcOp op;
} CalcOpFrame;

// Calculator state
typedef struct {
  // Mode
  bool rpn_mode;

  // Entry buffer
  char entry[CALC_DISPLAY_MAX + 1];
  int entry_len;
  bool has_dot;
  bool entering;       // true while user is typing digits

  // Standard mode: operator-precedence stack. Each frame is a deferred
  // (left-operand, operator) pair waiting for its right operand. On a new
  // operator, fold the top while its precedence allows; on '=', drain.
  CalcOpFrame op_stack[CALC_OP_STACK_DEPTH];
  int         op_stack_size;

  // Post-'=' "tape" view: snapshot of the just-completed expression (e.g.
  // "2 + 3 x 4 ="). Shown on the secondary line whenever the op_stack is
  // empty. Cleared by any subsequent user action that mutates X.
  char last_expression[CALC_SECONDARY_BUF_SIZE];

  // Standard mode: entry holds a freshly-computed value (unary result,
  // constant, MR) that should serve as the right operand for the next binary
  // operator. Distinguishes "post-unary, entering=false" from "just pressed an
  // operator, entering=false" — the latter wants change-my-mind, the former
  // wants fold-and-push.
  bool right_committed;

  // RPN stack (T, Z, Y, X — index 0=T, 3=X)
  double stack[4];
  bool stack_lift_enabled;  // next digit entry should lift the stack

  // Scientific calculator state
  int  page;            // 0=basic, 1=scientific, 2=memory/stack
  bool second_active;   // 2nd modifier sticky flag
  bool deg_mode;        // true=degrees, false=radians
  double memory;        // single memory register
  double last_x;        // HP-style LASTx

  // Error state
  bool error;
} CalcEngine;

// Initialize the calculator engine
void calc_engine_init(CalcEngine *engine);

// Set RPN mode on/off (resets the engine)
void calc_engine_set_rpn_mode(CalcEngine *engine, bool rpn);

// Process a button action
void calc_engine_handle_action(CalcEngine *engine, CalcAction action);

// Resolve an action through the 2nd-modifier (returns the inverse function
// when second_active, otherwise the action itself unchanged).
CalcAction calc_engine_resolve_2nd(CalcAction action, bool second_active);

// Get the formatted display string for the X register / current entry
const char *calc_engine_get_x_display(CalcEngine *engine);

// Get the display value for a given stack register (RPN mode)
// reg: 0=T, 1=Z, 2=Y (X is the entry display)
void calc_engine_get_stack_display(CalcEngine *engine, int reg, char *buf, int buf_size);

// Get the secondary display line for standard mode (pending operand + operator)
void calc_engine_get_secondary_display(CalcEngine *engine, char *buf, int buf_size);

// Get the main number (X register or current entry)
double calc_engine_get_main_number(CalcEngine *engine);

// Set the main number (X register or current entry)
void calc_engine_set_main_number(CalcEngine *engine, double val);
