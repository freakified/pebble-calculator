#include "calc_buttons.h"

// ---------------------------------------------------------------------------
// Layout constants for Emery (200 x 228)
// ---------------------------------------------------------------------------

// Display area occupies the top portion
#define DISPLAY_HEIGHT 52

// Button area starts below the display
#define BUTTON_AREA_Y DISPLAY_HEIGHT
#define BUTTON_AREA_H (228 - DISPLAY_HEIGHT) // 176 px

// Grid: 5 rows x 4 columns
#define GRID_ROWS 5
#define GRID_COLS 4

#define CELL_W (200 / GRID_COLS)           // 50 px
#define CELL_H (BUTTON_AREA_H / GRID_ROWS) // 35 px

// Helper to create a button rect from grid position
#define CELL_RECT(row, col, colspan)                                           \
  GRect((col) * CELL_W, BUTTON_AREA_Y + (row) * CELL_H, (colspan) * CELL_W,    \
        CELL_H)

// ---------------------------------------------------------------------------
// Static button definitions
// ---------------------------------------------------------------------------

static CalcButton s_buttons[CALC_BUTTON_COUNT] = {
    // Row 0: C, ±, %, ÷
    // In RPN: DROP, SWAP, %, ÷
    {
        .bounds = {.origin = {0, 0}, .size = {0, 0}}, // filled in init
        .label = "C",
        .rpn_label = "DROP",
        .action = CALC_ACTION_CLEAR,
        .rpn_action = CALC_ACTION_DROP,
        .style = BUTTON_STYLE_FUNCTION,
    },
    {
        .bounds = {.origin = {0, 0}, .size = {0, 0}},
        .label = "\xC2\xB1", // ± in UTF-8
        .rpn_label = "SWAP",
        .action = CALC_ACTION_NEGATE,
        .rpn_action = CALC_ACTION_SWAP,
        .style = BUTTON_STYLE_FUNCTION,
    },
    {
        .bounds = {.origin = {0, 0}, .size = {0, 0}},
        .label = "%",
        .rpn_label = NULL,
        .action = CALC_ACTION_PERCENT,
        .rpn_action = CALC_ACTION_PERCENT,
        .style = BUTTON_STYLE_FUNCTION,
    },
    {
        .bounds = {.origin = {0, 0}, .size = {0, 0}},
        .label = "\xC3\xB7", // ÷ in UTF-8
        .rpn_label = NULL,
        .action = CALC_ACTION_DIVIDE,
        .rpn_action = CALC_ACTION_DIVIDE,
        .style = BUTTON_STYLE_OPERATOR,
    },

    // Row 1: 7, 8, 9, ×
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
        .label = "\xC3\x97", // × in UTF-8
        .rpn_label = NULL,
        .action = CALC_ACTION_MULTIPLY,
        .rpn_action = CALC_ACTION_MULTIPLY,
        .style = BUTTON_STYLE_OPERATOR,
    },

    // Row 2: 4, 5, 6, −
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
        .label = "-", // minus
        .rpn_label = NULL,
        .action = CALC_ACTION_SUBTRACT,
        .rpn_action = CALC_ACTION_SUBTRACT,
        .style = BUTTON_STYLE_OPERATOR,
    },

    // Row 3: 1, 2, 3, +
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
        .label = "+",
        .rpn_label = NULL,
        .action = CALC_ACTION_ADD,
        .rpn_action = CALC_ACTION_ADD,
        .style = BUTTON_STYLE_OPERATOR,
    },

    // Row 4: 0 (wide), ., =  (in RPN: 0 (wide), ., ENT)
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
};

// ---------------------------------------------------------------------------
// Initialization — compute button rects
// ---------------------------------------------------------------------------

void calc_buttons_init(void) {
  int idx = 0;

  // Row 0: 4 buttons, each 1 column wide
  for (int col = 0; col < 4; col++) {
    s_buttons[idx].bounds = CELL_RECT(0, col, 1);
    idx++;
  }

  // Row 1: 4 buttons
  for (int col = 0; col < 4; col++) {
    s_buttons[idx].bounds = CELL_RECT(1, col, 1);
    idx++;
  }

  // Row 2: 4 buttons
  for (int col = 0; col < 4; col++) {
    s_buttons[idx].bounds = CELL_RECT(2, col, 1);
    idx++;
  }

  // Row 3: 4 buttons
  for (int col = 0; col < 4; col++) {
    s_buttons[idx].bounds = CELL_RECT(3, col, 1);
    idx++;
  }

  // Row 4: 0 (spans 2 cols), ., =
  s_buttons[idx].bounds = CELL_RECT(4, 0, 2); // "0" — 2 cols wide
  idx++;
  s_buttons[idx].bounds = CELL_RECT(4, 2, 1); // "."
  idx++;
  s_buttons[idx].bounds = CELL_RECT(4, 3, 1); // "=" / "ENT"
  idx++;
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
  for (int i = 0; i < CALC_BUTTON_COUNT; i++) {
    if (grect_contains_point(&s_buttons[i].bounds, &point)) {
      return i;
    }
  }
  return -1;
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
