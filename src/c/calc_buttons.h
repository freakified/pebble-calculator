#pragma once

#include <pebble.h>
#include "calc_engine.h"
#include "calc_icons.h"

// Button visual style
typedef enum {
  BUTTON_STYLE_NUMBER,    // Light gray bg, black text
  BUTTON_STYLE_OPERATOR,  // Orange bg, white text
  BUTTON_STYLE_ENTER,     // Special style for Enter/Equals
  BUTTON_STYLE_CLEAR,     // C / clear-X button: light gray bg, red text
} CalcButtonStyle;

// A single button definition
typedef struct {
  const int row;                 // Row (0 = top)
  const int col;                 // Column (0 = left)
  bool visible;                  // Whether the button should be drawn
  GRect bounds;                  // Pixel rect on screen
  const char *label;             // Label for standard mode (ignored if icon != NONE)
  CalcAction action;             // What action this button triggers
  CalcAction long_press_action;  // What action this button triggers when long-pressed
  CalcButtonStyle style;         // Visual style
  CalcIcon icon;                 // If set, drawn instead of label
} CalcButtonInfo;

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
#define CALC_MAX_BUTTONS_PER_GRID_CELL 4

typedef enum {
  CALC_BUTTON_NONE = -1,
  CALC_BUTTON_DIGIT_0,
  CALC_BUTTON_DIGIT_1,
  CALC_BUTTON_DIGIT_2,
  CALC_BUTTON_DIGIT_3,
  CALC_BUTTON_DIGIT_4,
  CALC_BUTTON_DIGIT_5,
  CALC_BUTTON_DIGIT_6,
  CALC_BUTTON_DIGIT_7,
  CALC_BUTTON_DIGIT_8,
  CALC_BUTTON_DIGIT_9,
  CALC_BUTTON_DOT,
  CALC_BUTTON_ADD,
  CALC_BUTTON_SUBTRACT,
  CALC_BUTTON_MULTIPLY,
  CALC_BUTTON_DIVIDE,

  CALC_BUTTON_EQUALS,
  CALC_BUTTON_ENTER,

  CALC_BUTTON_CLEAR_ALL, // Only in non-RPN mode
  CALC_BUTTON_BACKSPACE,

  CALC_BUTTON_COUNT,
} CalcButton;

// Initialize button layout (call once)
void calc_buttons_init(void);

// Get the button info for the given index
const CalcButtonInfo *calc_buttons_get_info(CalcButton btn);

// Get the vislble button for the given grid position
CalcButton calc_button_at_grid_position(int row, int col);

// Get total number of buttons
int calc_buttons_get_count(void);

// Hit-test: returns button id at the given point, or -1 if none
CalcButton calc_buttons_hit_test(GPoint point);

// Set whether a button should be visible
void calc_button_set_visible(CalcButton idx, bool visible);
