#include "calc_buttons.h"

// ---------------------------------------------------------------------------
// Page button tables. Per-page entries are 18 buttons: indices 0-15 are the
// 4×4 grid in row-major order on rectangular screens, index 16 is the C/CLR
// button, and index 17 is a platform extension slot used by the round basic ±
// key.
// ---------------------------------------------------------------------------

#if defined(PBL_ROUND)
#define CALC_ROUND_LAYOUT 1
#else
#define CALC_ROUND_LAYOUT 0
#endif

#if CALC_ROUND_LAYOUT
#define EXTRA_BASIC_STYLE BUTTON_STYLE_NUMBER
#define EXTRA_BASIC_ACTION CALC_ACTION_NEGATE
#define EXTRA_BASIC_LABEL "±"
#else
#define EXTRA_BASIC_STYLE BUTTON_STYLE_NONE
#define EXTRA_BASIC_ACTION CALC_ACTION_NOOP
#define EXTRA_BASIC_LABEL ""
#endif

#define ROUND_TOP_H 64
#define ROUND_DISPLAY_Y 16
#define ROUND_CELL_W CALC_CELL_W
#define ROUND_CELL_H CALC_CELL_H
#define ROUND_LEFT_RAIL_X 5
#define ROUND_CENTER_X 55
#define ROUND_RIGHT_RAIL_X 205

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
    // [17] round-only top-right ±
    { .label = EXTRA_BASIC_LABEL, .action = EXTRA_BASIC_ACTION, .rpn_action = EXTRA_BASIC_ACTION, .style = EXTRA_BASIC_STYLE, .icon = CALC_ICON_NONE },
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
    // [17] unused
    { .label = "", .action = CALC_ACTION_NOOP, .rpn_action = CALC_ACTION_NOOP, .style = BUTTON_STYLE_NONE },
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
    // [17] unused
    { .label = "", .action = CALC_ACTION_NOOP, .rpn_action = CALC_ACTION_NOOP, .style = BUTTON_STYLE_NONE },
  },
};

// Maps grid (row, col) -> button index. -1 = display cell (no hit).
// Same layout for every page (the C button is always cell (0, 0) and rows
// 1-4 cover indices 0-15 row-major). Mode-awareness for empty slots is
// applied via NOOP-action checks in hit_test below.
#if !CALC_ROUND_LAYOUT
static const int s_rect_cell_to_button[CALC_GRID_ROWS][CALC_GRID_COLS] = {
    {CALC_BUTTON_INDEX_CL, -1, -1, -1}, // C/CLR + display cells
    {0, 1, 2, 3},                       // grid row 1
    {4, 5, 6, 7},                       // grid row 2
    {8, 9, 10, 11},                     // grid row 3
    {12, 13, 14, 15},                   // grid row 4
};
#endif

#if CALC_ROUND_LAYOUT
static GRect prv_round_cell(int x, int row) {
  return GRect(x, ROUND_TOP_H + row * ROUND_CELL_H, ROUND_CELL_W,
               ROUND_CELL_H);
}

static GRect prv_round_basic_cell(int col, int row) {
  return prv_round_cell(ROUND_CENTER_X + col * ROUND_CELL_W, row);
}

static GRect prv_round_left_rail_cell(int row) {
  return prv_round_cell(ROUND_LEFT_RAIL_X, row);
}

static GRect prv_round_right_rail_cell(int row) {
  return prv_round_cell(ROUND_RIGHT_RAIL_X, row);
}

static GRect prv_round_five_slot_cell(int col, int row) {
  if (col <= 0) return prv_round_left_rail_cell(row);
  if (col >= 4) return prv_round_right_rail_cell(row);
  return prv_round_basic_cell(col - 1, row);
}

static void prv_set_bounds(int page, int index, GRect bounds) {
  s_buttons[page][index].bounds = bounds;
}
#endif

// ---------------------------------------------------------------------------
// Initialization — compute button rects (same bounds across all pages)
// ---------------------------------------------------------------------------

void calc_buttons_init(void) {
#if CALC_ROUND_LAYOUT
  for (int p = 0; p < CALC_PAGE_COUNT; p++) {
    for (int i = 0; i < CALC_BUTTON_COUNT; i++) {
      s_buttons[p][i].bounds = GRect(0, 0, 0, 0);
    }
    s_buttons[p][CALC_BUTTON_INDEX_CL].bounds = GRect(0, 0, 0, 0);
  }

  // Basic page: display-only top band, then 3-column number pad with
  // left/right rails.
  s_buttons[CALC_PAGE_BASIC][CALC_BUTTON_INDEX_CL].bounds =
      prv_round_left_rail_cell(0);
  prv_set_bounds(CALC_PAGE_BASIC, 0, prv_round_basic_cell(0, 0)); // 7
  prv_set_bounds(CALC_PAGE_BASIC, 1, prv_round_basic_cell(1, 0)); // 8
  prv_set_bounds(CALC_PAGE_BASIC, 2, prv_round_basic_cell(2, 0)); // 9
  prv_set_bounds(CALC_PAGE_BASIC, CALC_BUTTON_INDEX_EXTRA,
                 prv_round_right_rail_cell(0)); // ±

  prv_set_bounds(CALC_PAGE_BASIC, 3, prv_round_left_rail_cell(1)); // ÷
  prv_set_bounds(CALC_PAGE_BASIC, 4, prv_round_basic_cell(0, 1)); // 4
  prv_set_bounds(CALC_PAGE_BASIC, 5, prv_round_basic_cell(1, 1)); // 5
  prv_set_bounds(CALC_PAGE_BASIC, 6, prv_round_basic_cell(2, 1)); // 6
  prv_set_bounds(CALC_PAGE_BASIC, 11, prv_round_right_rail_cell(1)); // −

  prv_set_bounds(CALC_PAGE_BASIC, 7, prv_round_left_rail_cell(2)); // ×
  prv_set_bounds(CALC_PAGE_BASIC, 8, prv_round_basic_cell(0, 2)); // 1
  prv_set_bounds(CALC_PAGE_BASIC, 9, prv_round_basic_cell(1, 2)); // 2
  prv_set_bounds(CALC_PAGE_BASIC, 10, prv_round_basic_cell(2, 2)); // 3
  prv_set_bounds(CALC_PAGE_BASIC, 15, prv_round_right_rail_cell(2)); // +

  prv_set_bounds(CALC_PAGE_BASIC, 12, prv_round_basic_cell(0, 3)); // 0
  prv_set_bounds(CALC_PAGE_BASIC, 13, prv_round_basic_cell(1, 3)); // .
  prv_set_bounds(CALC_PAGE_BASIC, 14, prv_round_basic_cell(2, 3)); // =

  // Scientific page: keep the existing row-major order in the same five-slot
  // round rows, with C in the left rail.
  s_buttons[CALC_PAGE_SCI][CALC_BUTTON_INDEX_CL].bounds =
      prv_round_left_rail_cell(0);
  for (int i = 0; i < 16; i++) {
    int row = i / 4;
    int col = i % 4;
    prv_set_bounds(CALC_PAGE_SCI, i, prv_round_five_slot_cell(col + 1, row));
  }

  // Memory/RPN page: preserve the row-major page table in a round-safe,
  // five-slot grid.
  s_buttons[CALC_PAGE_MEM][CALC_BUTTON_INDEX_CL].bounds =
      prv_round_left_rail_cell(0);
  for (int i = 0; i < 16; i++) {
    int row = i / 4;
    int col = i % 4;
    prv_set_bounds(CALC_PAGE_MEM, i, prv_round_five_slot_cell(col + 1, row));
  }
#else
  for (int row = 0; row < CALC_GRID_ROWS; row++) {
    for (int col = 0; col < CALC_GRID_COLS; col++) {
      int idx = s_rect_cell_to_button[row][col];
      if (idx < 0) continue; // display cell
      GRect bounds = GRect(col * CALC_CELL_W, row * CALC_CELL_H + CALC_GRID_OFFSET_Y,
                           CALC_CELL_W, CALC_CELL_H);
      for (int p = 0; p < CALC_PAGE_COUNT; p++) {
        s_buttons[p][idx].bounds = bounds;
      }
    }
  }
  for (int p = 0; p < CALC_PAGE_COUNT; p++) {
    s_buttons[p][CALC_BUTTON_INDEX_EXTRA].bounds = GRect(0, 0, 0, 0);
  }
#endif
}

const CalcButton *calc_buttons_get(int page, int index) {
  if (page < 0 || page >= CALC_PAGE_COUNT) return NULL;
  if (index < 0 || index >= CALC_BUTTON_COUNT) return NULL;
  return &s_buttons[page][index];
}

int calc_buttons_get_count(void) { return CALC_BUTTON_COUNT; }

GRect calc_buttons_get_display_rect(GRect screen_bounds) {
#if CALC_ROUND_LAYOUT
  return GRect(58, ROUND_DISPLAY_Y, screen_bounds.size.w - 116,
               ROUND_TOP_H - ROUND_DISPLAY_Y);
#else
  return GRect(CALC_CELL_W, 0, screen_bounds.size.w - CALC_CELL_W,
               CALC_DISPLAY_HEIGHT + CALC_GRID_OFFSET_Y);
#endif
}

int calc_buttons_get_display_band_height(void) {
#if CALC_ROUND_LAYOUT
  return ROUND_TOP_H;
#else
  return CALC_DISPLAY_HEIGHT + CALC_GRID_OFFSET_Y;
#endif
}

GRect calc_buttons_get_button_draw_rect(const CalcButton *btn) {
  if (!btn) return GRect(0, 0, 0, 0);
  return grect_inset(btn->bounds, GEdgeInsets(2));
}

int calc_buttons_hit_test(int page, GPoint point, bool rpn_mode) {
  if (page < 0 || page >= CALC_PAGE_COUNT) return -1;
  for (int idx = 0; idx < CALC_BUTTON_COUNT; idx++) {
    CalcButton *btn = &s_buttons[page][idx];
    if (btn->style == BUTTON_STYLE_NONE) continue;
    CalcAction action = calc_button_get_action(btn, rpn_mode);
    if (action == CALC_ACTION_NOOP) continue;
    if (grect_contains_point(&btn->bounds, &point)) return idx;
  }
  return -1;
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
