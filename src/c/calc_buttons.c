#include "calc_buttons.h"

// ---------------------------------------------------------------------------
// Static button definitions
// ---------------------------------------------------------------------------
//
// Layout (RPN labels in parens where they differ):
//   C  [---------- display ----------]
//   7    8    9    ÷
//   4    5    6    ×
//   1    2    3    −
//   0    .    =(ENTER)    +
//
// C: clears in standard mode; in RPN mode acts as backspace while typing or
// clear-X (CLx) when no entry is in progress.
// Physical buttons: UP = SWAP (RPN only), DOWN = backspace, SELECT = ±.
static CalcButtonInfo s_buttons[CALC_BUTTON_COUNT] = {
    // Grid row 1: 7, 8, 9, ÷
    [CALC_BUTTON_DIGIT_7] = {
        .row = 1,
        .col = 0,
        .label = "7",
        .action = CALC_ACTION_DIGIT_7,
        .style = BUTTON_STYLE_NUMBER,
    },
    [CALC_BUTTON_DIGIT_8] = {
        .row = 1,
        .col = 1,
        .label = "8",
        .action = CALC_ACTION_DIGIT_8,
        .style = BUTTON_STYLE_NUMBER,
    },
    [CALC_BUTTON_DIGIT_9] = {
        .row = 1,
        .col = 2,
        .label = "9",
        .action = CALC_ACTION_DIGIT_9,
        .style = BUTTON_STYLE_NUMBER,
    },
    [CALC_BUTTON_DIVIDE] = {
        .row = 1,
        .col = 3,
        .label = "",
        .action = CALC_ACTION_DIVIDE,
        .style = BUTTON_STYLE_OPERATOR,
        .icon = CALC_ICON_DIVIDE,
    },

    // Grid row 2: 4, 5, 6, ×
    [CALC_BUTTON_DIGIT_4] = {
        .row = 2,
        .col = 0,
        .label = "4",
        .action = CALC_ACTION_DIGIT_4,
        .style = BUTTON_STYLE_NUMBER,
    },
    [CALC_BUTTON_DIGIT_5] = {
        .row = 2,
        .col = 1,
        .label = "5",
        .action = CALC_ACTION_DIGIT_5,
        .style = BUTTON_STYLE_NUMBER,
    },
    [CALC_BUTTON_DIGIT_6] = {
        .row = 2,
        .col = 2,
        .label = "6",
        .action = CALC_ACTION_DIGIT_6,
        .style = BUTTON_STYLE_NUMBER,
    },
    [CALC_BUTTON_MULTIPLY] = {
        .row = 2,
        .col = 3,
        .label = "",
        .action = CALC_ACTION_MULTIPLY,
        .style = BUTTON_STYLE_OPERATOR,
        .icon = CALC_ICON_MULTIPLY,
    },

    // Grid row 3: 1, 2, 3, −
    [CALC_BUTTON_DIGIT_1] = {
        .row = 3,
        .col = 0,
        .label = "1",
        .action = CALC_ACTION_DIGIT_1,
        .style = BUTTON_STYLE_NUMBER,
    },
    [CALC_BUTTON_DIGIT_2] = {
        .row = 3,
        .col = 1,
        .label = "2",
        .action = CALC_ACTION_DIGIT_2,
        .style = BUTTON_STYLE_NUMBER,
    },
    [CALC_BUTTON_DIGIT_3] = {
        .row = 3,
        .col = 2,
        .label = "3",
        .action = CALC_ACTION_DIGIT_3,
        .style = BUTTON_STYLE_NUMBER,
    },
    [CALC_BUTTON_SUBTRACT] = {
        .row = 3,
        .col = 3,
        .label = "",
        .action = CALC_ACTION_SUBTRACT,
        .style = BUTTON_STYLE_OPERATOR,
        .icon = CALC_ICON_MINUS,
    },

    // Grid row 4: 0, ., =(ENTER), +
    [CALC_BUTTON_DIGIT_0] = {
        .row = 4,
        .col = 0,
        .label = "0",
        .action = CALC_ACTION_DIGIT_0,
        .style = BUTTON_STYLE_NUMBER,
    },
    [CALC_BUTTON_DOT] = {
        .row = 4,
        .col = 1,
        .label = ".",
        .action = CALC_ACTION_DOT,
        .style = BUTTON_STYLE_NUMBER,
    },
    [CALC_BUTTON_EQUALS] = {
        .row = 4,
        .col = 2,
        .label = "",
        .action = CALC_ACTION_EQUALS,
        .style = BUTTON_STYLE_ENTER,
        .icon = CALC_ICON_EQUALS,
    },
    [CALC_BUTTON_ENTER] = {
        .row = 4,
        .col = 2,
        .label = "ENTER",
        .action = CALC_ACTION_ENTER,
        .style = BUTTON_STYLE_ENTER,
        .icon = CALC_ICON_NONE,
    },
    [CALC_BUTTON_ADD] = {
        .row = 4,
        .col = 3,
        .label = "",
        .action = CALC_ACTION_ADD,
        .style = BUTTON_STYLE_OPERATOR,
        .icon = CALC_ICON_PLUS,
    },

    [CALC_BUTTON_CLEAR_ALL] = {
        .row = 0,
        .col = 0,
        .label = "C",
        .action = CALC_ACTION_CLEAR_ALL,
        .style = BUTTON_STYLE_CLEAR,
        .icon = CALC_ICON_NONE,
    },
    [CALC_BUTTON_BACKSPACE] = {
        .row = 0,
        .col = 0,
        .label = "",
        .action = CALC_ACTION_BACKSPACE,
        .style = BUTTON_STYLE_CLEAR,
        .icon = CALC_ICON_BACKSPACE,
    },
};

// Maps grid (row, col) -> CalcButton.
static int8_t s_grid_cell_to_buttons[CALC_GRID_ROWS][CALC_GRID_COLS][CALC_MAX_BUTTONS_PER_GRID_CELL];

// ---------------------------------------------------------------------------
// Initialization — compute button rects
// ---------------------------------------------------------------------------

void calc_buttons_init(void) {
  memset(s_grid_cell_to_buttons, CALC_BUTTON_NONE, sizeof(s_grid_cell_to_buttons));
  for (int btn = 0; btn < CALC_BUTTON_COUNT; btn++) {
    CalcButtonInfo *info = &s_buttons[btn];
    s_buttons[btn].bounds =
        GRect(info->col * CALC_CELL_W, info->row * CALC_CELL_H + CALC_GRID_OFFSET_Y,
              CALC_CELL_W, CALC_CELL_H);
    for (int i = 0; i < CALC_MAX_BUTTONS_PER_GRID_CELL; i++) {
      if (s_grid_cell_to_buttons[info->row][info->col][i] == CALC_BUTTON_NONE) {
        s_grid_cell_to_buttons[info->row][info->col][i] = btn;
        break;
      }
    }
  }
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------

const CalcButtonInfo *calc_buttons_get_info(CalcButton btnidx) {
  if (btnidx < 0 || btnidx >= CALC_BUTTON_COUNT)
    return NULL;
  return &s_buttons[btnidx];
}

CalcButton calc_button_at_grid_position(int row, int col) {
  if (row < 0 || col < 0 || row >= CALC_GRID_ROWS || col >= CALC_GRID_COLS)
    return CALC_BUTTON_NONE;
  for (int i = 0; i < CALC_MAX_BUTTONS_PER_GRID_CELL; i++) {
    CalcButton btn = s_grid_cell_to_buttons[row][col][i];
    if (btn != CALC_BUTTON_NONE && s_buttons[btn].visible)
      return btn;
  }
  return CALC_BUTTON_NONE;
}

int calc_buttons_get_count(void) { return CALC_BUTTON_COUNT; }

CalcButton calc_buttons_hit_test(GPoint point) {
  if (point.x < 0 || point.y < CALC_GRID_OFFSET_Y)
    return CALC_BUTTON_NONE;
  int adj_y = point.y - CALC_GRID_OFFSET_Y;
  int col = point.x / CALC_CELL_W;
  int row = adj_y / CALC_CELL_H;
  if (col >= CALC_GRID_COLS)
    col = CALC_GRID_COLS - 1;
  if (row >= CALC_GRID_ROWS)
    row = CALC_GRID_ROWS - 1;
  return calc_button_at_grid_position(row, col);
}

void calc_button_set_visible(CalcButton idx, bool visible) {
  if (idx < 0 || idx >= CALC_BUTTON_COUNT) return;
  s_buttons[idx].visible = visible;
}
