#include "calc_ui.h"
#include "calc_buttons.h"
#include <stdio.h>

// ---------------------------------------------------------------------------
// Layout constants
// ---------------------------------------------------------------------------

#define DISPLAY_HEIGHT 52
#define DISPLAY_PAD_X  6
#define DISPLAY_PAD_Y  2

// Colors
#define COLOR_BG           GColorWhite
#define COLOR_DISPLAY_BG   GColorWhite
#define COLOR_DISPLAY_TEXT GColorBlack
#define COLOR_DISPLAY_SEC  GColorDarkGray
#define COLOR_SEPARATOR    GColorLightGray

#define COLOR_NUM_BG       GColorLightGray
#define COLOR_NUM_TEXT     GColorBlack
#define COLOR_NUM_PRESSED  GColorDarkGray

#define COLOR_OP_BG        GColorOrange
#define COLOR_OP_TEXT      GColorWhite
#define COLOR_OP_PRESSED   GColorWindsorTan

#define COLOR_FUNC_BG      GColorDarkGray
#define COLOR_FUNC_TEXT    GColorWhite
#define COLOR_FUNC_PRESSED GColorBlack

#define COLOR_GRID_BORDER  GColorDarkGray

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
  switch (btn->style) {
    case BUTTON_STYLE_NUMBER:
      *bg   = pressed ? COLOR_NUM_PRESSED : COLOR_NUM_BG;
      *text = pressed ? GColorWhite : COLOR_NUM_TEXT;
      break;
    case BUTTON_STYLE_OPERATOR:
      *bg   = pressed ? COLOR_OP_PRESSED : COLOR_OP_BG;
      *text = COLOR_OP_TEXT;
      break;
    case BUTTON_STYLE_FUNCTION:
      *bg   = pressed ? COLOR_FUNC_PRESSED : COLOR_FUNC_BG;
      *text = COLOR_FUNC_TEXT;
      break;
  }
}

static void prv_draw_display(GContext *ctx, GRect bounds) {
  if (!s_engine) return;

  // Background
  graphics_context_set_fill_color(ctx, COLOR_DISPLAY_BG);
  graphics_fill_rect(ctx, GRect(0, 0, bounds.size.w, DISPLAY_HEIGHT), 0, GCornerNone);

  bool rpn = s_engine->rpn_mode;

  if (rpn) {
    // RPN mode: show Y register (secondary) + X register (primary)
    GFont small_font = fonts_get_system_font(FONT_KEY_GOTHIC_18);
    GFont large_font = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);

    char buf[CALC_DISPLAY_MAX + 4];

    // Y register
    calc_engine_get_stack_display(s_engine, 2, buf, sizeof(buf));
    graphics_context_set_text_color(ctx, COLOR_DISPLAY_SEC);
    graphics_draw_text(ctx, buf, small_font,
        GRect(DISPLAY_PAD_X, DISPLAY_PAD_Y, bounds.size.w - DISPLAY_PAD_X * 2, 20),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
    graphics_draw_text(ctx, "Y:", small_font,
        GRect(DISPLAY_PAD_X, DISPLAY_PAD_Y, 22, 20),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

    // X register (primary)
    const char *x_str = calc_engine_get_x_display(s_engine);
    graphics_context_set_text_color(ctx, COLOR_DISPLAY_TEXT);
    graphics_draw_text(ctx, x_str, large_font,
        GRect(DISPLAY_PAD_X, DISPLAY_PAD_Y + 18, bounds.size.w - DISPLAY_PAD_X * 2, 32),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
    graphics_context_set_text_color(ctx, GColorDarkGray);
    graphics_draw_text(ctx, "X:", small_font,
        GRect(DISPLAY_PAD_X, DISPLAY_PAD_Y + 24, 22, 20),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  } else {
    // Standard mode: secondary line + primary line
    GFont large_font = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
    GFont small_font = fonts_get_system_font(FONT_KEY_GOTHIC_18);

    // Secondary (pending operand + operator)
    char sec_buf[32];
    calc_engine_get_secondary_display(s_engine, sec_buf, sizeof(sec_buf));
    graphics_context_set_text_color(ctx, COLOR_DISPLAY_SEC);
    graphics_draw_text(ctx, sec_buf, small_font,
        GRect(DISPLAY_PAD_X, DISPLAY_PAD_Y, bounds.size.w - DISPLAY_PAD_X * 2, 20),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);

    // Primary (current entry / result)
    const char *x_str = calc_engine_get_x_display(s_engine);
    graphics_context_set_text_color(ctx, COLOR_DISPLAY_TEXT);
    graphics_draw_text(ctx, x_str, large_font,
        GRect(DISPLAY_PAD_X, DISPLAY_PAD_Y + 18, bounds.size.w - DISPLAY_PAD_X * 2, 32),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
  }

  // Separator line
  graphics_context_set_stroke_color(ctx, COLOR_SEPARATOR);
  graphics_draw_line(ctx,
      GPoint(0, DISPLAY_HEIGHT - 1),
      GPoint(bounds.size.w, DISPLAY_HEIGHT - 1));
}

static void prv_draw_buttons(GContext *ctx, GRect bounds) {
  int count = calc_buttons_get_count();
  bool rpn = s_engine ? s_engine->rpn_mode : false;

  GFont label_font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  GFont small_label_font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);

  for (int i = 0; i < count; i++) {
    const CalcButton *btn = calc_buttons_get(i);
    if (!btn) continue;

    bool pressed = (i == s_pressed_index);
    GColor bg, text;
    prv_get_button_colors(btn, pressed, &bg, &text);

    // Fill button background
    graphics_context_set_fill_color(ctx, bg);
    GRect btn_rect = btn->bounds;
    // Inset slightly for grid gap effect
    GRect fill_rect = grect_inset(btn_rect, GEdgeInsets(1));
    graphics_fill_rect(ctx, fill_rect, 3, GCornersAll);

    // Draw label
    const char *label = calc_button_get_label(btn, rpn);
    graphics_context_set_text_color(ctx, text);

    // Use smaller font for multi-char labels like "DROP", "SWAP", "ENT"
    GFont font = (strlen(label) > 2) ? small_label_font : label_font;

    // Center text in button
    int text_h = (font == small_label_font) ? 20 : 26;
    int text_y = btn_rect.origin.y + (btn_rect.size.h - text_h) / 2 - 2;
    graphics_draw_text(ctx, label, font,
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

void calc_ui_set_pressed(int button_index) {
  s_pressed_index = button_index;
}

void calc_ui_set_engine(CalcEngine *engine) {
  s_engine = engine;
}

void calc_ui_mark_dirty(void) {
  if (s_ui_layer) {
    layer_mark_dirty(s_ui_layer);
  }
}
