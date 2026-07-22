#pragma once

#include <pebble.h>
#include "calc_engine.h"
#include "calc_icons.h"

// Button visual style
typedef enum {
  BUTTON_STYLE_NUMBER,    // Light gray bg, black text — digits and constants (π/e)
  BUTTON_STYLE_OPERATOR,  // Orange bg, white text — binary ops
  BUTTON_STYLE_ENTER,     // Special style for Enter/Equals
  BUTTON_STYLE_CLEAR,     // C / clear-X button: light gray bg, red text
  BUTTON_STYLE_FUNC,      // Sci/memory/stack functions — muted blue-gray bg
  BUTTON_STYLE_MOD,       // 2nd modifier — yellow when active, FUNC otherwise
  BUTTON_STYLE_DRG,       // DEG/RAD toggle — vivid cerulean bg, black text
  BUTTON_STYLE_NONE,      // Empty slot — not drawn, not pressable
} CalcButtonStyle;

// A single button definition
typedef struct {
  GRect bounds;              // Pixel rect on screen
  const char *label;         // Label for standard mode (ignored if icon != NONE)
  const char *rpn_label;     // Label for RPN mode (NULL = same as label)
  const char *second_label;  // Label when 2nd modifier active (NULL = same as label)
  CalcAction action;         // What action this button triggers
  CalcAction rpn_action;     // Action in RPN mode (if different; use same action if equal)
  CalcButtonStyle style;     // Visual style
  CalcIcon icon;             // If set, drawn instead of label
} CalcButton;

// Layout grid (5 rows x 4 cols on Emery 200x228).
// Row 0: [DEL][          display          ]
// Rows 1-4: number/operator buttons.
#define CALC_GRID_ROWS 5
#define CALC_GRID_COLS 4
#define CALC_CELL_W 50              // 200 / 4
#define CALC_CELL_H 45              // 5*45 = 225; 3px slack absorbed into row 0 top
#define CALC_GRID_OFFSET_Y 3        // pushes grid down so the 3px gap is at the top
#define CALC_DISPLAY_HEIGHT CALC_CELL_H

// Number of buttons per page (16 grid slots + 1 C/CLR slot + 1 platform
// extension slot). The extension slot is only active on round basic layouts.
#define CALC_BUTTON_COUNT 18

// Index of the C / clear-X button (lives in grid cell (0, 0)).
// Fires CLEAR in standard mode; in RPN mode acts as backspace while typing
// or clear-X (CLx) when no entry is in progress.
#define CALC_BUTTON_INDEX_CL 16
#define CALC_BUTTON_INDEX_EXTRA 17

// Initialize button layout (call once)
void calc_buttons_init(void);

// Get a button on the given page at the given button index.
const CalcButton *calc_buttons_get(int page, int index);

// Get total number of button slots per page.
int calc_buttons_get_count(void);

// Get the display text rect for the current platform.
GRect calc_buttons_get_display_rect(GRect screen_bounds);

// Get the height of the display band background for the current platform.
int calc_buttons_get_display_band_height(void);

// Get the inset visual rect for a button. The button's bounds remain the
// larger logical hit target.
GRect calc_buttons_get_button_draw_rect(const CalcButton *btn);

// Hit-test: returns button index at the given point on the given page (and
// for the current mode), or -1 if none. Mode-awareness lets us hide buttons
// that are NOOP in the current mode (e.g. RPN stack ops in standard mode).
int calc_buttons_hit_test(int page, GPoint point, bool rpn_mode);

// Get the effective label for a button given the current mode and 2nd state.
const char *calc_button_get_label(const CalcButton *btn, bool rpn_mode, bool second_active);

// Get the effective action for a button given the current mode (caller is
// responsible for applying 2nd-modifier resolution via calc_engine_resolve_2nd).
CalcAction calc_button_get_action(const CalcButton *btn, bool rpn_mode);
