#pragma once

#include <pebble.h>
#include "calc_engine.h"

// Button visual style
typedef enum {
  BUTTON_STYLE_NUMBER,    // Light gray bg, black text
  BUTTON_STYLE_OPERATOR,  // Orange bg, white text
  BUTTON_STYLE_FUNCTION,  // Dark gray bg, white text
  BUTTON_STYLE_ENTER,     // Special style for Enter/Equals
  BUTTON_STYLE_CLEAR,     // AC/DROP button: light gray bg, red text
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

// Layout grid (5 rows x 4 cols on Emery 200x228).
// Row 0: [DEL][          display          ]
// Rows 1-4: number/operator buttons. Bottom 3px of the screen are slack
// and absorbed into row 4 by the hit test.
#define CALC_GRID_ROWS 5
#define CALC_GRID_COLS 4
#define CALC_CELL_W 50              // 200 / 4
#define CALC_CELL_H 45              // 5*45 = 225; 3px slack absorbed into row 0 top
#define CALC_GRID_OFFSET_Y 3        // pushes grid down so the 3px gap is at the top
#define CALC_DISPLAY_HEIGHT CALC_CELL_H

// Number of buttons (16 number/operator buttons + 1 C/DROP).
#define CALC_BUTTON_COUNT 17

// Index of the C/DROP button (lives in grid cell (0, 0)).
// Fires CLEAR in standard mode, DROP in RPN mode.
#define CALC_BUTTON_INDEX_CL 16

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
