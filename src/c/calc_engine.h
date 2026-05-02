#pragma once

#include <pebble.h>

// Maximum characters in the display buffer (must hold 13 digits + sign + dot + null)
#define CALC_DISPLAY_MAX 16

// Max digit characters (0-9) that fit in the X display with LECO 32 Bold.
// The decimal point is narrower and doesn't consume a digit slot.
// The minus sign DOES consume one digit slot.
#define CALC_X_MAX_DIGITS_LECO 7

// Max digit characters that fit with GOTHIC 28 Bold (used when overflowing LECO).
// Beyond this count, scientific notation kicks in.
#define CALC_X_MAX_DIGITS_GOTHIC 13

// Max characters for scientific notation display (rendered in GOTHIC 28 Bold)
#define CALC_SCI_MAX_CHARS 10

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
  CALC_ACTION_PERCENT,
  CALC_ACTION_NEGATE,
  CALC_ACTION_EQUALS,    // Standard mode: evaluate
  CALC_ACTION_CLEAR,     // C
  CALC_ACTION_BACKSPACE, // Remove last char from current entry
  // RPN-specific
  CALC_ACTION_ENTER,     // Push X onto stack
  CALC_ACTION_SWAP,      // Swap X <-> Y
  CALC_ACTION_DROP,      // Drop X
} CalcAction;

// Pending operator for standard mode
typedef enum {
  CALC_OP_NONE = 0,
  CALC_OP_ADD,
  CALC_OP_SUBTRACT,
  CALC_OP_MULTIPLY,
  CALC_OP_DIVIDE,
} CalcOp;

// Calculator state
typedef struct {
  // Mode
  bool rpn_mode;

  // Entry buffer
  char entry[CALC_DISPLAY_MAX + 1];
  int entry_len;
  bool has_dot;
  bool entering;       // true while user is typing digits

  // Standard mode state
  double pending_value;
  CalcOp pending_op;

  // RPN stack (T, Z, Y, X — index 0=T, 3=X)
  double stack[4];
  bool stack_lift_enabled;  // next digit entry should lift the stack

  // Error state
  bool error;
} CalcEngine;

// Initialize the calculator engine
void calc_engine_init(CalcEngine *engine);

// Set RPN mode on/off (resets the engine)
void calc_engine_set_rpn_mode(CalcEngine *engine, bool rpn);

// Process a button action
void calc_engine_handle_action(CalcEngine *engine, CalcAction action);

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
