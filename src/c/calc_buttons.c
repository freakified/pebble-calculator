#include "calc_buttons.h"

// ---------------------------------------------------------------------------
// Static button definitions
// ---------------------------------------------------------------------------
//
// Layout (RPN labels in parens where they differ):
//   C(DROP)  [---------- display ----------]
//   7    8    9    ÷
//   4    5    6    ×
//   1    2    3    −
//   0    .    =(ENTER)    +
//
// C/DROP: fires CLEAR (standard) or DROP (RPN).
// Physical buttons: UP = SWAP (RPN only).
static CalcButton s_buttons[CALC_BUTTON_COUNT] = {
    // Grid row 1: 7, 8, 9, ÷
    {
        .bounds = {.origin = {0, 0}, .size = {0, 0}},
        .label = "7",
        .rpn_label = NULL,
        .action = CALC_ACTION_DIGIT_7,
        .rpn_action = CALC_ACTION_DIGIT_7,
        .style = BUTTON_STYLE_NUMBER,
    },
    {
        .bounds = {.origin = {0, 0}, .size = {0, 0}},
        .label = "8",
        .rpn_label = NULL,
        .action = CALC_ACTION_DIGIT_8,
        .rpn_action = CALC_ACTION_DIGIT_8,
        .style = BUTTON_STYLE_NUMBER,
    },
    {
        .bounds = {.origin = {0, 0}, .size = {0, 0}},
        .label = "9",
        .rpn_label = NULL,
        .action = CALC_ACTION_DIGIT_9,
        .rpn_action = CALC_ACTION_DIGIT_9,
        .style = BUTTON_STYLE_NUMBER,
    },
    {
        .bounds = {.origin = {0, 0}, .size = {0, 0}},
        .label = "",
        .rpn_label = NULL,
        .action = CALC_ACTION_DIVIDE,
        .rpn_action = CALC_ACTION_DIVIDE,
        .style = BUTTON_STYLE_OPERATOR,
        .icon = CALC_ICON_DIVIDE,
    },

    // Grid row 2: 4, 5, 6, ×
    {
        .bounds = {.origin = {0, 0}, .size = {0, 0}},
        .label = "4",
        .rpn_label = NULL,
        .action = CALC_ACTION_DIGIT_4,
        .rpn_action = CALC_ACTION_DIGIT_4,
        .style = BUTTON_STYLE_NUMBER,
    },
    {
        .bounds = {.origin = {0, 0}, .size = {0, 0}},
        .label = "5",
        .rpn_label = NULL,
        .action = CALC_ACTION_DIGIT_5,
        .rpn_action = CALC_ACTION_DIGIT_5,
        .style = BUTTON_STYLE_NUMBER,
    },
    {
        .bounds = {.origin = {0, 0}, .size = {0, 0}},
        .label = "6",
        .rpn_label = NULL,
        .action = CALC_ACTION_DIGIT_6,
        .rpn_action = CALC_ACTION_DIGIT_6,
        .style = BUTTON_STYLE_NUMBER,
    },
    {
        .bounds = {.origin = {0, 0}, .size = {0, 0}},
        .label = "",
        .rpn_label = NULL,
        .action = CALC_ACTION_MULTIPLY,
        .rpn_action = CALC_ACTION_MULTIPLY,
        .style = BUTTON_STYLE_OPERATOR,
        .icon = CALC_ICON_MULTIPLY,
    },

    // Grid row 3: 1, 2, 3, −
    {
        .bounds = {.origin = {0, 0}, .size = {0, 0}},
        .label = "1",
        .rpn_label = NULL,
        .action = CALC_ACTION_DIGIT_1,
        .rpn_action = CALC_ACTION_DIGIT_1,
        .style = BUTTON_STYLE_NUMBER,
    },
    {
        .bounds = {.origin = {0, 0}, .size = {0, 0}},
        .label = "2",
        .rpn_label = NULL,
        .action = CALC_ACTION_DIGIT_2,
        .rpn_action = CALC_ACTION_DIGIT_2,
        .style = BUTTON_STYLE_NUMBER,
    },
    {
        .bounds = {.origin = {0, 0}, .size = {0, 0}},
        .label = "3",
        .rpn_label = NULL,
        .action = CALC_ACTION_DIGIT_3,
        .rpn_action = CALC_ACTION_DIGIT_3,
        .style = BUTTON_STYLE_NUMBER,
    },
    {
        .bounds = {.origin = {0, 0}, .size = {0, 0}},
        .label = "",
        .rpn_label = NULL,
        .action = CALC_ACTION_SUBTRACT,
        .rpn_action = CALC_ACTION_SUBTRACT,
        .style = BUTTON_STYLE_OPERATOR,
        .icon = CALC_ICON_MINUS,
    },

    // Grid row 4: 0, ., =(ENTER), +
    {
        .bounds = {.origin = {0, 0}, .size = {0, 0}},
        .label = "0",
        .rpn_label = NULL,
        .action = CALC_ACTION_DIGIT_0,
        .rpn_action = CALC_ACTION_DIGIT_0,
        .style = BUTTON_STYLE_NUMBER,
    },
    {
        .bounds = {.origin = {0, 0}, .size = {0, 0}},
        .label = ".",
        .rpn_label = NULL,
        .action = CALC_ACTION_DOT,
        .rpn_action = CALC_ACTION_DOT,
        .style = BUTTON_STYLE_NUMBER,
    },
    {
        .bounds = {.origin = {0, 0}, .size = {0, 0}},
        .label = "=",
        .rpn_label = "ENTER",
        .action = CALC_ACTION_EQUALS,
        .rpn_action = CALC_ACTION_ENTER,
        .style = BUTTON_STYLE_ENTER,
    },
    {
        .bounds = {.origin = {0, 0}, .size = {0, 0}},
        .label = "",
        .rpn_label = NULL,
        .action = CALC_ACTION_ADD,
        .rpn_action = CALC_ACTION_ADD,
        .style = BUTTON_STYLE_OPERATOR,
        .icon = CALC_ICON_PLUS,
    },

    // C/DROP button (index 16) — placed in grid cell (0, 0) by
    // calc_buttons_init.
    {
        .bounds = {.origin = {0, 0}, .size = {0, 0}},
        .label = "",
        .rpn_label = NULL,
        .action = CALC_ACTION_CLEAR,
        .rpn_action = CALC_ACTION_CLEAR,
        .style = BUTTON_STYLE_CLEAR,
        .icon = CALC_ICON_BACKSPACE,
    },
};

// Maps grid (row, col) -> button index. -1 = display cell (no hit).
static const int s_grid_cell_to_button[CALC_GRID_ROWS][CALC_GRID_COLS] = {
    {CALC_BUTTON_INDEX_CL, -1, -1, -1}, // C/DROP, display, display, display
    {0, 1, 2, 3},                       // 7 8 9 ÷
    {4, 5, 6, 7},                       // 4 5 6 ×
    {8, 9, 10, 11},                     // 1 2 3 −
    {12, 13, 14, 15},                   // 0 . = +
};

// ---------------------------------------------------------------------------
// Initialization — compute button rects
// ---------------------------------------------------------------------------

void calc_buttons_init(void) {
  for (int row = 0; row < CALC_GRID_ROWS; row++) {
    for (int col = 0; col < CALC_GRID_COLS; col++) {
      int idx = s_grid_cell_to_button[row][col];
      if (idx < 0)
        continue; // display cell
      s_buttons[idx].bounds =
          GRect(col * CALC_CELL_W, row * CALC_CELL_H + CALC_GRID_OFFSET_Y,
                CALC_CELL_W, CALC_CELL_H);
    }
  }
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------

const CalcButton *calc_buttons_get(int index) {
  if (index < 0 || index >= CALC_BUTTON_COUNT)
    return NULL;
  return &s_buttons[index];
}

int calc_buttons_get_count(void) { return CALC_BUTTON_COUNT; }

int calc_buttons_hit_test(GPoint point) {
  if (point.x < 0 || point.y < CALC_GRID_OFFSET_Y)
    return -1;
  int adj_y = point.y - CALC_GRID_OFFSET_Y;
  int col = point.x / CALC_CELL_W;
  int row = adj_y / CALC_CELL_H;
  if (col >= CALC_GRID_COLS)
    col = CALC_GRID_COLS - 1;
  if (row >= CALC_GRID_ROWS)
    row = CALC_GRID_ROWS - 1;
  return s_grid_cell_to_button[row][col];
}

const char *calc_button_get_label(const CalcButton *btn, bool rpn_mode) {
  if (rpn_mode && btn->rpn_label != NULL) {
    return btn->rpn_label;
  }
  return btn->label;
}

CalcAction calc_button_get_action(const CalcButton *btn, bool rpn_mode) {
  if (rpn_mode) {
    return btn->rpn_action;
  }
  return btn->action;
}
