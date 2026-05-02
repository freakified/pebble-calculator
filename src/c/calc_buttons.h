#pragma once

#include <pebble.h>
#include "calc_engine.h"

// Button visual style
typedef enum {
  BUTTON_STYLE_NUMBER,    // Light gray bg, black text
  BUTTON_STYLE_OPERATOR,  // Orange bg, white text
  BUTTON_STYLE_FUNCTION,  // Dark gray bg, white text
  BUTTON_STYLE_ENTER,     // Special style for Enter/Equals
} CalcButtonStyle;

// A single button definition
typedef struct {
  GRect bounds;              // Pixel rect on screen
  const char *label;         // Label for standard mode
  const char *rpn_label;     // Label for RPN mode (NULL = same as label)
  CalcAction action;         // What action this button triggers
  CalcAction rpn_action;     // Action in RPN mode (if different; use same action if equal)
  CalcButtonStyle style;     // Visual style
} CalcButton;

// Number of buttons
#define CALC_BUTTON_COUNT 16

// Initialize button layout (call once)
void calc_buttons_init(void);

// Get the button at the given index
const CalcButton *calc_buttons_get(int index);

// Get total number of buttons
int calc_buttons_get_count(void);

// Hit-test: returns button index at the given point, or -1 if none
int calc_buttons_hit_test(GPoint point);

// Get the effective label for a button given the current mode
const char *calc_button_get_label(const CalcButton *btn, bool rpn_mode);

// Get the effective action for a button given the current mode
CalcAction calc_button_get_action(const CalcButton *btn, bool rpn_mode);
