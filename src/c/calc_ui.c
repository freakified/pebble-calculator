#include "calc_ui.h"
#include "calc_buttons.h"
#include "calc_fonts.h"
#include <stdio.h>

// ---------------------------------------------------------------------------
// Layout constants (grid dimensions live in calc_buttons.h)
// ---------------------------------------------------------------------------

#define DISPLAY_PAD_X 6
#define DISPLAY_PAD_Y 2

// Colors
#define COLOR_BG GColorBlack
#define COLOR_DISPLAY_BG GColorWhite
#define COLOR_DISPLAY_TEXT GColorBlack
#define COLOR_DISPLAY_SEC GColorDarkGray

#define COLOR_NUM_BG GColorDarkGray
#define COLOR_NUM_TEXT GColorWhite

#define COLOR_OP_BG GColorOrange
#define COLOR_OP_TEXT GColorWhite

#define COLOR_FUNC_BG GColorCobaltBlue
#define COLOR_FUNC_TEXT GColorWhite

#define COLOR_ENT_BG GColorBlue
#define COLOR_ENT_TEXT GColorWhite

#define COLOR_GRID_BORDER GColorWhite

// ---------------------------------------------------------------------------
// Static state
// ---------------------------------------------------------------------------

static Layer *s_ui_layer = NULL;
static CalcEngine *s_engine = NULL;
static int s_pressed_index = -1;

// ---------------------------------------------------------------------------
// Drawing helpers
// ---------------------------------------------------------------------------

static void prv_get_button_colors(const CalcButton *btn, bool pressed,
                                  GColor *bg, GColor *text) {
  if (pressed) {
    *bg = GColorWhite;
    *text = GColorBlack;
    return;
  }

  switch (btn->style) {
  case BUTTON_STYLE_NUMBER:
    *bg = COLOR_NUM_BG;
    *text = COLOR_NUM_TEXT;
    break;
  case BUTTON_STYLE_OPERATOR:
    *bg = COLOR_OP_BG;
    *text = COLOR_OP_TEXT;
    break;
  case BUTTON_STYLE_FUNCTION:
    *bg = COLOR_FUNC_BG;
    *text = COLOR_FUNC_TEXT;
    break;
  case BUTTON_STYLE_ENTER:
    *bg = COLOR_ENT_BG;
    *text = COLOR_ENT_TEXT;
    break;
  }
}

static void prv_draw_display(GContext *ctx, GRect bounds) {
  if (!s_engine)
    return;

  // Display occupies row 0, cols 1-3. (Cell (0,0) is the DEL button.)
  const int disp_x = CALC_CELL_W;
  const int disp_w = bounds.size.w - CALC_CELL_W;

  graphics_context_set_fill_color(ctx, COLOR_DISPLAY_BG);
  graphics_fill_rect(ctx, GRect(disp_x, 0, disp_w, CALC_DISPLAY_HEIGHT), 0,
                     GCornerNone);

  const CalcFonts *fonts = calc_fonts_get();

  const int text_left = disp_x + DISPLAY_PAD_X;
  const int text_w = disp_w - 2 * DISPLAY_PAD_X;

  // Secondary line: Y register (RPN) or pending operand + operator (standard).
  char sec_buf[CALC_DISPLAY_MAX + 4];
  if (s_engine->rpn_mode) {
    calc_engine_get_stack_display(s_engine, 2, sec_buf, sizeof(sec_buf));
  } else {
    calc_engine_get_secondary_display(s_engine, sec_buf, sizeof(sec_buf));
  }
  graphics_context_set_text_color(ctx, COLOR_DISPLAY_SEC);
  // Negative y trims GOTHIC's top padding so the line sits flush at the top.
  graphics_draw_text(
      ctx, sec_buf, fonts->indicator, GRect(text_left, -4, text_w, 18),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);

  // Primary line: X register, large.
  const char *x_str = calc_engine_get_x_display(s_engine);
  graphics_context_set_text_color(ctx, COLOR_DISPLAY_TEXT);
  graphics_draw_text(
      ctx, x_str, fonts->x_register, GRect(text_left, 8, text_w, 32),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
}

static void prv_draw_buttons(GContext *ctx, GRect bounds) {
  int count = calc_buttons_get_count();
  bool rpn = s_engine ? s_engine->rpn_mode : false;

  const CalcFonts *fonts = calc_fonts_get();

  for (int i = 0; i < count; i++) {
    const CalcButton *btn = calc_buttons_get(i);
    if (!btn)
      continue;

    bool pressed = (i == s_pressed_index);
    GColor bg, text;
    prv_get_button_colors(btn, pressed, &bg, &text);

    // Fill button background
    graphics_context_set_fill_color(ctx, bg);
    GRect btn_rect = btn->bounds;
    // Inset slightly for grid gap effect
    GRect fill_rect = grect_inset(btn_rect, GEdgeInsets(2));
    graphics_fill_rect(ctx, fill_rect, 5, GCornersAll);

    // Draw label
    const char *label = calc_button_get_label(btn, rpn);
    graphics_context_set_text_color(ctx, text);

    GFont font;
    int text_h;
    int y_offset;

    if (btn->style == BUTTON_STYLE_NUMBER) {
      font = fonts->button_num;
      text_h = 32;
      y_offset = -6;
    } else if (strlen(label) > 2) {
      font = fonts->button_label_small;
      text_h = 18;
      y_offset = -2;
    } else {
      font = fonts->button_label;
      text_h = 24;
      y_offset = -4;
    }

    // Center text in button
    int text_y = btn_rect.origin.y + (btn_rect.size.h - text_h) / 2 + y_offset;
    graphics_draw_text(
        ctx, label, font,
        GRect(btn_rect.origin.x, text_y, btn_rect.size.w, text_h),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }
}

// ---------------------------------------------------------------------------
// Layer update proc
// ---------------------------------------------------------------------------

static void prv_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

  // Clear background
  graphics_context_set_fill_color(ctx, COLOR_BG);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  // Draw display area
  prv_draw_display(ctx, bounds);

  // Draw button grid
  prv_draw_buttons(ctx, bounds);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

Layer *calc_ui_create(GRect bounds) {
  s_ui_layer = layer_create(bounds);
  layer_set_update_proc(s_ui_layer, prv_update_proc);
  return s_ui_layer;
}

void calc_ui_destroy(Layer *layer) {
  if (layer) {
    layer_destroy(layer);
  }
  if (s_ui_layer == layer) {
    s_ui_layer = NULL;
  }
}

void calc_ui_set_pressed(int button_index) { s_pressed_index = button_index; }

void calc_ui_set_engine(CalcEngine *engine) { s_engine = engine; }

void calc_ui_mark_dirty(void) {
  if (s_ui_layer) {
    layer_mark_dirty(s_ui_layer);
  }
}
