#include "calc_buttons.h"

// ---------------------------------------------------------------------------
// Page button tables. Per-page entries are 17 buttons: indices 0-15 are the
// 4×4 grid in row-major order (rows 1-4 of the screen), index 16 is the C/CLR
// button in cell (0, 0).
// ---------------------------------------------------------------------------

static CalcButton s_buttons[CALC_PAGE_COUNT][CALC_BUTTON_COUNT] = {

  // ===========================================================================
  // Page 0 — Basic (digits + ÷ × − +, =/ENTER)
  // ===========================================================================
  {
    // Row 1: 7, 8, 9, ÷
    { .label = "7", .action = CALC_ACTION_DIGIT_7, .rpn_action = CALC_ACTION_DIGIT_7, .style = BUTTON_STYLE_NUMBER },
    { .label = "8", .action = CALC_ACTION_DIGIT_8, .rpn_action = CALC_ACTION_DIGIT_8, .style = BUTTON_STYLE_NUMBER },
    { .label = "9", .action = CALC_ACTION_DIGIT_9, .rpn_action = CALC_ACTION_DIGIT_9, .style = BUTTON_STYLE_NUMBER },
    { .label = "", .action = CALC_ACTION_DIVIDE, .rpn_action = CALC_ACTION_DIVIDE, .style = BUTTON_STYLE_OPERATOR, .icon = CALC_ICON_DIVIDE },
    // Row 2: 4, 5, 6, ×
    { .label = "4", .action = CALC_ACTION_DIGIT_4, .rpn_action = CALC_ACTION_DIGIT_4, .style = BUTTON_STYLE_NUMBER },
    { .label = "5", .action = CALC_ACTION_DIGIT_5, .rpn_action = CALC_ACTION_DIGIT_5, .style = BUTTON_STYLE_NUMBER },
    { .label = "6", .action = CALC_ACTION_DIGIT_6, .rpn_action = CALC_ACTION_DIGIT_6, .style = BUTTON_STYLE_NUMBER },
    { .label = "", .action = CALC_ACTION_MULTIPLY, .rpn_action = CALC_ACTION_MULTIPLY, .style = BUTTON_STYLE_OPERATOR, .icon = CALC_ICON_MULTIPLY },
    // Row 3: 1, 2, 3, −
    { .label = "1", .action = CALC_ACTION_DIGIT_1, .rpn_action = CALC_ACTION_DIGIT_1, .style = BUTTON_STYLE_NUMBER },
    { .label = "2", .action = CALC_ACTION_DIGIT_2, .rpn_action = CALC_ACTION_DIGIT_2, .style = BUTTON_STYLE_NUMBER },
    { .label = "3", .action = CALC_ACTION_DIGIT_3, .rpn_action = CALC_ACTION_DIGIT_3, .style = BUTTON_STYLE_NUMBER },
    { .label = "", .action = CALC_ACTION_SUBTRACT, .rpn_action = CALC_ACTION_SUBTRACT, .style = BUTTON_STYLE_OPERATOR, .icon = CALC_ICON_MINUS },
    // Row 4: 0, ., =/ENTER, +
    { .label = "0", .action = CALC_ACTION_DIGIT_0, .rpn_action = CALC_ACTION_DIGIT_0, .style = BUTTON_STYLE_NUMBER },
    { .label = ".", .action = CALC_ACTION_DOT, .rpn_action = CALC_ACTION_DOT, .style = BUTTON_STYLE_NUMBER },
    { .label = "", .rpn_label = "ENTER", .action = CALC_ACTION_EQUALS, .rpn_action = CALC_ACTION_ENTER, .style = BUTTON_STYLE_ENTER, .icon = CALC_ICON_EQUALS },
    { .label = "", .action = CALC_ACTION_ADD, .rpn_action = CALC_ACTION_ADD, .style = BUTTON_STYLE_OPERATOR, .icon = CALC_ICON_PLUS },
    // [16] C/DEL
    { .label = "C", .action = CALC_ACTION_CLEAR, .rpn_action = CALC_ACTION_CLEAR, .style = BUTTON_STYLE_CLEAR, .icon = CALC_ICON_NONE },
  },

  // ===========================================================================
  // Page 1 — Scientific. Same buttons in both modes (the scientific page
  // is mode-agnostic).
  // ===========================================================================
  {
    // Row 1: 2nd, sin, cos, tan
    { .label = "2nd", .action = CALC_ACTION_2ND_TOGGLE, .rpn_action = CALC_ACTION_2ND_TOGGLE, .style = BUTTON_STYLE_MOD },
    { .label = "sin", .second_label = "asin", .action = CALC_ACTION_SIN, .rpn_action = CALC_ACTION_SIN, .style = BUTTON_STYLE_FUNC },
    { .label = "cos", .second_label = "acos", .action = CALC_ACTION_COS, .rpn_action = CALC_ACTION_COS, .style = BUTTON_STYLE_FUNC },
    { .label = "tan", .second_label = "atan", .action = CALC_ACTION_TAN, .rpn_action = CALC_ACTION_TAN, .style = BUTTON_STYLE_FUNC },
    // Row 2: ln, log, √, x²
    { .label = "ln", .second_label = "eˣ", .action = CALC_ACTION_LN, .rpn_action = CALC_ACTION_LN, .style = BUTTON_STYLE_FUNC },
    { .label = "log", .second_label = "10ˣ", .action = CALC_ACTION_LOG10, .rpn_action = CALC_ACTION_LOG10, .style = BUTTON_STYLE_FUNC },
    { .label = "√", .second_label = "x³", .action = CALC_ACTION_SQRT, .rpn_action = CALC_ACTION_SQRT, .style = BUTTON_STYLE_FUNC },
    { .label = "x²", .second_label = "³√x", .action = CALC_ACTION_SQUARE, .rpn_action = CALC_ACTION_SQUARE, .style = BUTTON_STYLE_FUNC },
    // Row 3: 1/x, y^x, π, e
    { .label = "1/x", .second_label = "x!", .action = CALC_ACTION_RECIP, .rpn_action = CALC_ACTION_RECIP, .style = BUTTON_STYLE_FUNC },
    { .label = "yˣ", .second_label = "ˣ√y", .action = CALC_ACTION_POW, .rpn_action = CALC_ACTION_POW, .style = BUTTON_STYLE_OPERATOR },
    { .label = "π", .action = CALC_ACTION_PI, .rpn_action = CALC_ACTION_PI, .style = BUTTON_STYLE_NUMBER },
    { .label = "e", .action = CALC_ACTION_E, .rpn_action = CALC_ACTION_E, .style = BUTTON_STYLE_NUMBER },
    // Row 4: ±, empty, empty, empty
    { .label = "±", .action = CALC_ACTION_NEGATE, .rpn_action = CALC_ACTION_NEGATE, .style = BUTTON_STYLE_NUMBER },
    { .label = "", .action = CALC_ACTION_NOOP, .rpn_action = CALC_ACTION_NOOP, .style = BUTTON_STYLE_NONE },
    { .label = "", .action = CALC_ACTION_NOOP, .rpn_action = CALC_ACTION_NOOP, .style = BUTTON_STYLE_NONE },
    { .label = "", .action = CALC_ACTION_NOOP, .rpn_action = CALC_ACTION_NOOP, .style = BUTTON_STYLE_NONE },
    // [16] C/DEL
    { .label = "C", .action = CALC_ACTION_CLEAR, .rpn_action = CALC_ACTION_CLEAR, .style = BUTTON_STYLE_CLEAR, .icon = CALC_ICON_NONE },
  },

  // ===========================================================================
  // Page 2 — Memory + RPN stack. Memory row is shared; stack ops only show
  // in RPN mode (they map to NOOP in standard, hidden by hit-test/draw).
  // ===========================================================================
  {
    // Row 1: M+, M−, MR, MC (both modes)
    { .label = "M+", .action = CALC_ACTION_M_PLUS, .rpn_action = CALC_ACTION_M_PLUS, .style = BUTTON_STYLE_FUNC },
    { .label = "M−", .action = CALC_ACTION_M_MINUS, .rpn_action = CALC_ACTION_M_MINUS, .style = BUTTON_STYLE_FUNC },
    { .label = "MR", .action = CALC_ACTION_M_RECALL, .rpn_action = CALC_ACTION_M_RECALL, .style = BUTTON_STYLE_FUNC },
    { .label = "MC", .action = CALC_ACTION_M_CLEAR, .rpn_action = CALC_ACTION_M_CLEAR, .style = BUTTON_STYLE_FUNC },
    // Row 2: R↓, R↑, x↔y, DROP — RPN only
    { .label = "", .rpn_label = "R↓", .action = CALC_ACTION_NOOP, .rpn_action = CALC_ACTION_ROLL_DOWN, .style = BUTTON_STYLE_FUNC },
    { .label = "", .rpn_label = "R↑", .action = CALC_ACTION_NOOP, .rpn_action = CALC_ACTION_ROLL_UP, .style = BUTTON_STYLE_FUNC },
    { .label = "", .rpn_label = "x↔y", .action = CALC_ACTION_NOOP, .rpn_action = CALC_ACTION_SWAP, .style = BUTTON_STYLE_FUNC },
    { .label = "", .rpn_label = "DROP", .action = CALC_ACTION_NOOP, .rpn_action = CALC_ACTION_DROP, .style = BUTTON_STYLE_FUNC },
    // Row 3: CLST, LASTx, empty, empty — RPN only for first two
    { .label = "", .rpn_label = "CLST", .action = CALC_ACTION_NOOP, .rpn_action = CALC_ACTION_STACK_CLEAR, .style = BUTTON_STYLE_FUNC },
    { .label = "", .rpn_label = "LASTx", .action = CALC_ACTION_NOOP, .rpn_action = CALC_ACTION_LAST_X, .style = BUTTON_STYLE_FUNC },
    { .label = "", .action = CALC_ACTION_NOOP, .rpn_action = CALC_ACTION_NOOP, .style = BUTTON_STYLE_NONE },
    { .label = "", .action = CALC_ACTION_NOOP, .rpn_action = CALC_ACTION_NOOP, .style = BUTTON_STYLE_NONE },
    // Row 4: all empty
    { .label = "", .action = CALC_ACTION_NOOP, .rpn_action = CALC_ACTION_NOOP, .style = BUTTON_STYLE_NONE },
    { .label = "", .action = CALC_ACTION_NOOP, .rpn_action = CALC_ACTION_NOOP, .style = BUTTON_STYLE_NONE },
    { .label = "", .action = CALC_ACTION_NOOP, .rpn_action = CALC_ACTION_NOOP, .style = BUTTON_STYLE_NONE },
    { .label = "", .action = CALC_ACTION_NOOP, .rpn_action = CALC_ACTION_NOOP, .style = BUTTON_STYLE_NONE },
    // [16] C/DEL
    { .label = "C", .action = CALC_ACTION_CLEAR, .rpn_action = CALC_ACTION_CLEAR, .style = BUTTON_STYLE_CLEAR, .icon = CALC_ICON_NONE },
  },
};

// Maps grid (row, col) -> button index. -1 = display cell (no hit).
// Same layout for every page (the C button is always cell (0, 0) and rows
// 1-4 cover indices 0-15 row-major). Mode-awareness for empty slots is
// applied via NOOP-action checks in hit_test below.
static const int s_grid_cell_to_button[CALC_GRID_ROWS][CALC_GRID_COLS] = {
    {CALC_BUTTON_INDEX_CL, -1, -1, -1}, // C/CLR + display cells
    {0, 1, 2, 3},                       // grid row 1
    {4, 5, 6, 7},                       // grid row 2
    {8, 9, 10, 11},                     // grid row 3
    {12, 13, 14, 15},                   // grid row 4
};

// ---------------------------------------------------------------------------
// Initialization — compute button rects (same bounds across all pages)
// ---------------------------------------------------------------------------

void calc_buttons_init(void) {
  for (int row = 0; row < CALC_GRID_ROWS; row++) {
    for (int col = 0; col < CALC_GRID_COLS; col++) {
      int idx = s_grid_cell_to_button[row][col];
      if (idx < 0) continue; // display cell
      GRect bounds = GRect(col * CALC_CELL_W, row * CALC_CELL_H + CALC_GRID_OFFSET_Y,
                           CALC_CELL_W, CALC_CELL_H);
      for (int p = 0; p < CALC_PAGE_COUNT; p++) {
        s_buttons[p][idx].bounds = bounds;
      }
    }
  }
}

const CalcButton *calc_buttons_get(int page, int index) {
  if (page < 0 || page >= CALC_PAGE_COUNT) return NULL;
  if (index < 0 || index >= CALC_BUTTON_COUNT) return NULL;
  return &s_buttons[page][index];
}

int calc_buttons_get_count(void) { return CALC_BUTTON_COUNT; }

int calc_buttons_hit_test(int page, GPoint point, bool rpn_mode) {
  if (page < 0 || page >= CALC_PAGE_COUNT) return -1;
  if (point.x < 0 || point.y < CALC_GRID_OFFSET_Y) return -1;
  int adj_y = point.y - CALC_GRID_OFFSET_Y;
  int col = point.x / CALC_CELL_W;
  int row = adj_y / CALC_CELL_H;
  if (col >= CALC_GRID_COLS) col = CALC_GRID_COLS - 1;
  if (row >= CALC_GRID_ROWS) row = CALC_GRID_ROWS - 1;
  int idx = s_grid_cell_to_button[row][col];
  if (idx < 0) return -1;
  // Mode-aware hide: if the action resolves to NOOP for this mode, the cell
  // is empty here (e.g., RPN-only stack ops in standard mode).
  CalcAction action = calc_button_get_action(&s_buttons[page][idx], rpn_mode);
  if (action == CALC_ACTION_NOOP) return -1;
  return idx;
}

const char *calc_button_get_label(const CalcButton *btn, bool rpn_mode, bool second_active) {
  if (second_active && btn->second_label != NULL) {
    return btn->second_label;
  }
  if (rpn_mode && btn->rpn_label != NULL) {
    return btn->rpn_label;
  }
  return btn->label;
}

CalcAction calc_button_get_action(const CalcButton *btn, bool rpn_mode) {
  if (rpn_mode) return btn->rpn_action;
  return btn->action;
}
