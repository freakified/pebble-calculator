#include "calc_ui.h"
#include "calc_buttons.h"
#include "calc_fonts.h"
#include "calc_icons.h"
#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Layout constants (grid dimensions live in calc_buttons.h)
// ---------------------------------------------------------------------------

#define DISPLAY_PAD_X 6
#define DISPLAY_PAD_Y 2

// Page-indicator dot geometry — centered along the very top edge of the
// screen, above the display text, so it doesn't fight the memory chip or
// the right-aligned secondary text.
#define PAGE_DOT_RADIUS 2
#define PAGE_DOT_SPACING 7
#define PAGE_DOT_TOP_Y 3

// Button state-indicator dot geometry — drawn above the label on the INV
// and DEG/RAD buttons in place of the display-area chips they replaced.
#define STATE_DOT_RADIUS 2
#define STATE_DOT_SPACING 8
#define STATE_DOT_TOP_MARGIN 7

// Roll direction indicator — a small filled triangle near the bottom of the
// ROLL button, pointing the direction the stack rolls.
#define STATE_TRIANGLE_HALF_W 7
#define STATE_TRIANGLE_H 6
#define STATE_TRIANGLE_BOTTOM_MARGIN 7

// Press-preview bubble geometry. It's a little larger than a key's draw rect
// (46x41 on Emery) and floats BUBBLE_LIFT px above the key so the fingertip
// doesn't cover it. Where there's no room for that (top rows, round bezel) it
// slides down toward the key, as far as overlapping its top edge by
// BUBBLE_OVERLAP; beyond that (the C key) it moves BUBBLE_LIFT px beside the
// key instead. BUBBLE_MAX_NUDGE bounds the inward slide used to keep it clear
// of the edges (and the bezel on round).
#define BUBBLE_W 56
#define BUBBLE_H 50
#define BUBBLE_LIFT 20
#define BUBBLE_OVERLAP 6
#define BUBBLE_BORDER 2
#define BUBBLE_MAX_NUDGE 48

// Page-flip hint (see calc_ui_set_hint): the target page's keypad slides in
// over the current one, its leading edge marked by a HINT_EDGE_W px line.
#define HINT_EDGE_W 2

// Colors
#define COLOR_BG GColorBlack
#define COLOR_DISPLAY_BG GColorWhite
#define COLOR_DISPLAY_TEXT GColorBlack
#define COLOR_DISPLAY_SEC GColorDarkGray

#define COLOR_NUM_BG GColorDarkGray
#define COLOR_NUM_TEXT GColorWhite

#define COLOR_OP_BG GColorOrange
#define COLOR_OP_TEXT GColorWhite

#define COLOR_CLEAR_BG GColorMelon
#define COLOR_CLEAR_TEXT GColorDarkCandyAppleRed

#define COLOR_ENT_BG GColorBlue
#define COLOR_ENT_TEXT GColorWhite

// Scientific unary functions (sin/log/etc.) and memory/stack ops.
#define COLOR_FUNC_BG GColorMidnightGreen
#define COLOR_FUNC_TEXT GColorWhite

// 2nd modifier (INV) — tiffany blue when idle, yellow when toggled on.
#define COLOR_MOD_BG GColorTiffanyBlue
#define COLOR_MOD_TEXT GColorBlack
#define COLOR_MOD_ACTIVE_BG GColorYellow
#define COLOR_MOD_ACTIVE_TEXT GColorBlack

// DEG/RAD toggle.
#define COLOR_DRG_BG GColorTiffanyBlue
#define COLOR_DRG_TEXT GColorBlack

// Page-indicator dot colors.
#define COLOR_DOT_INACTIVE GColorLightGray
#define COLOR_DOT_ACTIVE GColorBlack

// Page-flip hint: the incoming keypad's leading edge.
#define COLOR_HINT_EDGE GColorLightGray

// Button state-indicator dot color (INV on/off; DEG/RAD selector) — filled
// when selected, a hollow ring of the same color otherwise.
#define COLOR_STATE_DOT GColorBlack

// ---------------------------------------------------------------------------
// Static state
// ---------------------------------------------------------------------------

static Layer *s_ui_layer = NULL;
static CalcEngine *s_engine = NULL;
static int s_pressed_index = -1;
static CalcGestureHint s_hint = CALC_HINT_NONE;
static int s_hint_travel = 0;

// ---------------------------------------------------------------------------
// Drawing helpers
// ---------------------------------------------------------------------------

// If str has a fractional part whose tail is 4+ identical digits, write a
// shortened version into buf that fills up to max_digits total digits, then
// return buf. Otherwise returns str unchanged. Only useful for period-1
// repeating decimals.
static const char *prv_shorten_repeating(const char *str, char *buf,
                                         int buf_size, int max_digits) {
  const char *dot = strchr(str, '.');
  if (!dot)
    return str;
  const char *frac = dot + 1;
  int frac_len = (int)strlen(frac);
  if (frac_len < 5)
    return str;

  char last = frac[frac_len - 1];
  int run = 0;
  for (int i = frac_len - 1; i >= 0 && frac[i] == last; i--)
    run++;
  if (run < 4) {
    // The final digit may be the rounded last place of a repeating tail
    // (e.g. 2/3 → 0.666666666667) — retry ignoring it.
    if (frac_len < 6)
      return str;
    last = frac[frac_len - 2];
    run = 0;
    for (int i = frac_len - 2; i >= 0 && frac[i] == last; i--)
      run++;
    if (run < 4)
      return str;
  }

  int int_digit_count = 0;
  for (const char *p = str; p < dot; p++) {
    if (*p >= '0' && *p <= '9')
      int_digit_count++;
  }

  int keep_frac = max_digits - int_digit_count;
  if (keep_frac < 1)
    keep_frac = 1;
  if (keep_frac >= frac_len)
    return str;

  int total = (int)(dot - str) + 1 + keep_frac; // +1 for '.'
  if (total >= buf_size)
    return str;

  memcpy(buf, str, total);
  buf[total] = '\0';
  return buf;
}

static void prv_get_button_colors(const CalcButton *btn, bool pressed,
                                  bool second_active, GColor *bg,
                                  GColor *text) {
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
  case BUTTON_STYLE_ENTER:
    *bg = COLOR_ENT_BG;
    *text = COLOR_ENT_TEXT;
    break;
  case BUTTON_STYLE_CLEAR:
    *bg = COLOR_CLEAR_BG;
    *text = COLOR_CLEAR_TEXT;
    break;
  case BUTTON_STYLE_FUNC:
    *bg = COLOR_FUNC_BG;
    *text = COLOR_FUNC_TEXT;
    break;
  case BUTTON_STYLE_MOD:
    if (second_active) {
      *bg = COLOR_MOD_ACTIVE_BG;
      *text = COLOR_MOD_ACTIVE_TEXT;
    } else {
      *bg = COLOR_MOD_BG;
      *text = COLOR_MOD_TEXT;
    }
    break;
  case BUTTON_STYLE_DRG:
    *bg = COLOR_DRG_BG;
    *text = COLOR_DRG_TEXT;
    break;
  case BUTTON_STYLE_NONE:
  default:
    *bg = COLOR_BG;
    *text = COLOR_BG;
    break;
  }
}

static void prv_draw_page_indicator(GContext *ctx, GRect bounds,
                                    int active_page) {
  const int span = (CALC_PAGE_COUNT - 1) * PAGE_DOT_SPACING;
  const int x_base = bounds.size.w / 2 - span / 2;
  const int y = PAGE_DOT_TOP_Y;
  for (int i = 0; i < CALC_PAGE_COUNT; i++) {
    GColor c = (i == active_page) ? COLOR_DOT_ACTIVE : COLOR_DOT_INACTIVE;
    graphics_context_set_fill_color(ctx, c);
    int x = x_base + i * PAGE_DOT_SPACING;
    graphics_fill_circle(ctx, GPoint(x, y), PAGE_DOT_RADIUS);
  }
}

static void prv_draw_chips(GContext *ctx, GRect display_rect) {
  if (!s_engine)
    return;
  const CalcFonts *fonts = calc_fonts_get();

  // Chip text — M whenever active (INV and DEG/RAD are shown on their own
  // buttons via indicator dots, not here).
  char buf[16];
  buf[0] = '\0';

  if (s_engine->memory != 0.0) {
    strcat(buf, "M");
  }

  if (buf[0] == '\0')
    return;

  graphics_context_set_text_color(ctx, COLOR_DISPLAY_SEC);
  graphics_draw_text(
      ctx, buf, fonts->indicator,
      GRect(display_rect.origin.x + 2, display_rect.origin.y + 1, 60, 18),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}

static void prv_draw_display(GContext *ctx, GRect bounds) {
  if (!s_engine)
    return;

  // White display bg spans the full top band; top-band buttons are drawn over
  // it. Text occupies the layout-provided display rect, right-aligned.
  graphics_context_set_fill_color(ctx, COLOR_DISPLAY_BG);
  graphics_fill_rect(
      ctx, GRect(0, 0, bounds.size.w, calc_buttons_get_display_band_height()),
      0, GCornerNone);

  const CalcFonts *fonts = calc_fonts_get();

  GRect display_rect = calc_buttons_get_display_rect(bounds);
  const int text_left = display_rect.origin.x;
  const int text_w = display_rect.size.w - DISPLAY_PAD_X;

  // Page dots sit centered along the very top edge; the memory chip occupies
  // the left side of the secondary-line row.
  prv_draw_page_indicator(ctx, bounds, s_engine->page);
  prv_draw_chips(ctx, display_rect);

  // Secondary line: Y register (RPN) or pending operand + operator (standard).
  // Skipped while typing — the X register's primary line conveys the entry.
  char sec_buf[CALC_SECONDARY_BUF_SIZE];
  if (s_engine->rpn_mode) {
    calc_engine_get_stack_display(s_engine, 2, sec_buf, sizeof(sec_buf));
  } else {
    calc_engine_get_secondary_display(s_engine, sec_buf, sizeof(sec_buf));
  }

  // Reserve enough horizontal space on the left for the page dots + chip text
  // before clipping the secondary value into the remaining width.
  const int sec_left_reserved = 38;
  const int sec_x = text_left + sec_left_reserved;
  const int sec_w = text_w - sec_left_reserved - 2;

  graphics_context_set_text_color(ctx, COLOR_DISPLAY_SEC);
  graphics_draw_text(ctx, sec_buf, fonts->indicator,
                     GRect(sec_x, display_rect.origin.y + 1, sec_w, 18),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentRight,
                     NULL);

  // Primary line: X register, large.
  // Font tiers: LECO 32 Bold if the string's measured width fits, else GOTHIC
  // 28 Bold (whose metrics the formatter/entry caps are derived from, so it
  // always fits — the trailing-ellipsis mode below should never fire).
  // Repeating-decimal tails are shortened first so they don't needlessly push
  // the display into the compact font.
  const char *x_str = calc_engine_get_x_display(s_engine);

  char x_short_buf[CALC_DISPLAY_MAX + 1];
  const char *x_display = x_str;
  bool use_compact_font = s_engine->error || strchr(x_str, 'e') != NULL;
  if (!use_compact_font) {
    const GRect measure_box = GRect(0, 0, 500, 40);
    GSize sz = graphics_text_layout_get_content_size(
        x_display, fonts->x_register, measure_box, GTextOverflowModeWordWrap,
        GTextAlignmentLeft);
    if (sz.w > text_w) {
      x_display = prv_shorten_repeating(x_str, x_short_buf, sizeof(x_short_buf),
                                        CALC_X_MAX_DIGITS_LECO);
      if (x_display != x_str) {
        sz = graphics_text_layout_get_content_size(
            x_display, fonts->x_register, measure_box,
            GTextOverflowModeWordWrap, GTextAlignmentLeft);
      }
      use_compact_font = sz.w > text_w;
      if (use_compact_font) {
        // The compact font fits the full string — no need to show the
        // shortened form there.
        x_display = x_str;
      }
    }
  }
  GFont x_font = use_compact_font
                     ? fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD)
                     : fonts->x_register;
  graphics_context_set_text_color(ctx, s_engine->error ? GColorDarkCandyAppleRed
                                                       : COLOR_DISPLAY_TEXT);
  graphics_draw_text(
      ctx, x_display, x_font,
      GRect(text_left - (use_compact_font ? 2 : 0),
            display_rect.origin.y + 13 + (use_compact_font ? 2 : 0), text_w,
            32),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
}

// A button state-indicator dot: always has a 1px black stroke; filled solid
// black when active/selected, transparent (hollow ring) otherwise.
static void prv_draw_state_dot(GContext *ctx, GPoint center, bool filled) {
  if (filled) {
    graphics_context_set_fill_color(ctx, COLOR_STATE_DOT);
    graphics_fill_circle(ctx, center, STATE_DOT_RADIUS);
  }
  graphics_context_set_stroke_color(ctx, COLOR_STATE_DOT);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_circle(ctx, center, STATE_DOT_RADIUS);
}

// A small filled direction triangle for the ROLL buttons, cached like the
// backspace icon's GPath. Points are relative to a (0,0) vertical center;
// gpath_move_to() below repositions it per draw.
static GPathInfo s_triangle_down_info = {
    .num_points = 3,
    .points = (GPoint[]){{0, STATE_TRIANGLE_H},
                         {-STATE_TRIANGLE_HALF_W, 0},
                         {STATE_TRIANGLE_HALF_W, 0}},
};
static GPathInfo s_triangle_up_info = {
    .num_points = 3,
    .points = (GPoint[]){{0, -STATE_TRIANGLE_H},
                         {-STATE_TRIANGLE_HALF_W, 0},
                         {STATE_TRIANGLE_HALF_W, 0}},
};
static GPath *s_triangle_down_path = NULL;
static GPath *s_triangle_up_path = NULL;

static void prv_draw_state_triangle(GContext *ctx, GPoint center, bool down) {
  GPath **path = down ? &s_triangle_down_path : &s_triangle_up_path;
  if (!*path) {
    *path = gpath_create(down ? &s_triangle_down_info : &s_triangle_up_info);
  }
  gpath_move_to(*path, center);
  graphics_context_set_fill_color(ctx, GColorWhite);
  gpath_draw_filled(ctx, *path);
}

// Draws a key's face (label, icon, state indicators) into rect, over a bg the
// caller has already filled. Shared by the grid and the press-preview bubble.
static void prv_draw_button_content(GContext *ctx, const CalcButton *btn,
                                    int index, GRect fill_rect, GColor bg,
                                    GColor text, bool second) {
  bool rpn = s_engine->rpn_mode;
  CalcAction effective_action = calc_button_get_action(btn, rpn);
  const CalcFonts *fonts = calc_fonts_get();

  // C/DEL button: dynamic icon or label based on engine entry state.
  if (index == CALC_BUTTON_INDEX_CL) {
    if (s_engine->entering) {
      calc_icons_draw(ctx, CALC_ICON_BACKSPACE, fill_rect, text, bg);
    } else {
      const char *clabel = s_engine->just_cleared ? "AC" : "C";
      int text_y = fill_rect.origin.y + (fill_rect.size.h - 24) / 2 - 6;
      graphics_context_set_text_color(ctx, text);
      graphics_draw_text(
          ctx, clabel, fonts->button_label,
          GRect(fill_rect.origin.x, text_y, fill_rect.size.w, 24),
          GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    }
    return;
  }

  // Icons take precedence over labels, except in RPN mode when an explicit
  // rpn_label is set (e.g. ENTER overrides the = icon).
  bool show_icon =
      (btn->icon != CALC_ICON_NONE) && !(rpn && btn->rpn_label != NULL);
  if (show_icon) {
    calc_icons_draw(ctx, btn->icon, fill_rect, text, bg);
    return;
  }

  // Draw label
  const char *label = calc_button_get_label(btn, rpn, second);
  // DRG key shows the CURRENT angle mode; pressing it flips.
  if (effective_action == CALC_ACTION_DRG_TOGGLE) {
    label = s_engine->deg_mode ? "DEG" : "RAD";
  }
  graphics_context_set_text_color(ctx, text);

  GFont font;
  int text_h;
  int y_offset;

  // CALC_ACTION_DIGIT_0 is 0 and DOT immediately follows DIGIT_9, so this
  // range check covers exactly the digit and decimal-point keys.
  bool is_digit_key = effective_action <= CALC_ACTION_DOT;
  // INV/DEG/RAD/→HMS/CLST/ROLL read better a size down even though they'd
  // fit at the big size — everything else falls back to the small font
  // only if the big-font rendering would overflow the button, so any
  // future long label (ENTER, DROP, LASTx, ...) self-adjusts without
  // needing to be special-cased here.
  bool force_small_font = effective_action == CALC_ACTION_2ND_TOGGLE ||
                          effective_action == CALC_ACTION_DRG_TOGGLE ||
                          effective_action == CALC_ACTION_TO_HMS ||
                          effective_action == CALC_ACTION_STACK_CLEAR ||
                          effective_action == CALC_ACTION_ROLL_DOWN ||
                          effective_action == CALC_ACTION_ROLL_UP;
  if (is_digit_key) {
    // Digit and decimal-point keys get the big LECO number font.
    font = fonts->button_num;
    text_h = 32;
    y_offset = -5;
  } else {
    const GRect measure_box = GRect(0, 0, 200, 40);
    GSize label_sz = graphics_text_layout_get_content_size(
        label, fonts->button_label, measure_box, GTextOverflowModeWordWrap,
        GTextAlignmentCenter);
    if (force_small_font || label_sz.w > fill_rect.size.w) {
      font = fonts->button_label_small;
      text_h = 18;
      y_offset = -2;
    } else {
      font = fonts->button_label;
      text_h = 24;
      y_offset = -6;
    }
  }

  // Center text in button
  int text_y = fill_rect.origin.y + (fill_rect.size.h - text_h) / 2 + y_offset;
  graphics_draw_text(
      ctx, label, font,
      GRect(fill_rect.origin.x, text_y, fill_rect.size.w, text_h),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // State-indicator dots, replacing the INV/DEG-RAD chips that used to
  // live in the display area.
  int dot_y = fill_rect.origin.y + STATE_DOT_TOP_MARGIN;
  int center_x = fill_rect.origin.x + fill_rect.size.w / 2;
  if (effective_action == CALC_ACTION_2ND_TOGGLE) {
    // INV: one dot, filled when toggled on, hollow ring otherwise.
    prv_draw_state_dot(ctx, GPoint(center_x, dot_y), second);
  } else if (effective_action == CALC_ACTION_DRG_TOGGLE) {
    // DEG/RAD: two dots, the active mode's dot is filled; the other is a
    // hollow ring.
    int deg_x = center_x - STATE_DOT_SPACING / 2;
    int rad_x = center_x + STATE_DOT_SPACING / 2;
    prv_draw_state_dot(ctx, GPoint(deg_x, dot_y), s_engine->deg_mode);
    prv_draw_state_dot(ctx, GPoint(rad_x, dot_y), !s_engine->deg_mode);
  } else if (effective_action == CALC_ACTION_ROLL_DOWN ||
             effective_action == CALC_ACTION_ROLL_UP) {
    // ROLL: a small triangle near the bottom points the roll direction.
    // The down/up chevrons aren't vertically symmetric around their
    // anchor point, so each gets its own nudge to land in the same spot.
    bool down = effective_action == CALC_ACTION_ROLL_DOWN;
    int tri_y = fill_rect.origin.y + fill_rect.size.h -
                STATE_TRIANGLE_BOTTOM_MARGIN + (down ? -3 : 2);
    prv_draw_state_triangle(ctx, GPoint(center_x, tri_y), down);
  }
}

// Draws page's keys shifted x_off px sideways, skipping any that start above
// min_y. The grid draws the current page in place; the page-flip hint draws
// the incoming page's keypad sliding in.
static void prv_draw_buttons(GContext *ctx, int page, int x_off, int min_y,
                             bool second) {
  int count = calc_buttons_get_count();
  bool rpn = s_engine->rpn_mode;

  for (int i = 0; i < count; i++) {
    const CalcButton *btn = calc_buttons_get(page, i);
    if (!btn || btn->bounds.origin.y < min_y)
      continue;

    // Skip buttons that are NONE-styled or NOOP for current mode (empty slots).
    CalcAction effective_action = calc_button_get_action(btn, rpn);
    if (btn->style == BUTTON_STYLE_NONE ||
        effective_action == CALC_ACTION_NOOP) {
      continue;
    }

    GColor bg, text;
    bool pressed = page == s_engine->page && i == s_pressed_index;
    prv_get_button_colors(btn, pressed, second, &bg, &text);

    // Fill button background
    graphics_context_set_fill_color(ctx, bg);
    GRect fill_rect = calc_buttons_get_button_draw_rect(btn);
    fill_rect.origin.x += x_off;
    graphics_fill_rect(ctx, fill_rect, 8, GCornersAll);

    prv_draw_button_content(ctx, btn, i, fill_rect, bg, text, second);
  }
}

// Whether rect lies entirely on the visible screen. On round displays the
// corners must also fall inside the circle (less a small margin).
static bool prv_rect_on_screen(GRect r, GRect bounds) {
  if (r.origin.x < 0 || r.origin.y < 0 ||
      r.origin.x + r.size.w > bounds.size.w ||
      r.origin.y + r.size.h > bounds.size.h) {
    return false;
  }
#if defined(PBL_ROUND)
  const int cx = bounds.size.w / 2;
  const int cy = bounds.size.h / 2;
  const int radius = bounds.size.w / 2 - 2;
  const int xs[2] = {r.origin.x, r.origin.x + r.size.w};
  const int ys[2] = {r.origin.y, r.origin.y + r.size.h};
  for (int i = 0; i < 2; i++) {
    for (int j = 0; j < 2; j++) {
      int dx = xs[i] - cx;
      int dy = ys[j] - cy;
      if (dx * dx + dy * dy > radius * radius) {
        return false;
      }
    }
  }
#endif
  return true;
}

// Try to place r on screen: from its preferred spot, slide it down (to at
// most max_y) and horizontally toward the screen's center (by at most
// BUBBLE_MAX_NUDGE px), taking the first position that fits — so it stays as
// close to where it was asked to be as possible. Returns false if none fits.
static bool prv_fit_bubble(GRect *r, GRect bounds, int max_y) {
  const int cx = bounds.size.w / 2;
  const int center = r->origin.x + r->size.w / 2;
  const int dir = (center < cx) ? 1 : -1;
  int y = r->origin.y < 0 ? 0 : r->origin.y;
  for (; y <= max_y; y += 2) {
    for (int nudge = 0; nudge <= BUBBLE_MAX_NUDGE; nudge += 2) {
      GRect t = GRect(r->origin.x + dir * nudge, y, r->size.w, r->size.h);
      if (prv_rect_on_screen(t, bounds)) {
        *r = t;
        return true;
      }
    }
  }
  return false;
}

// Where a bubble of size sz goes for anchor (a key's draw rect): above it, or
// beside it where there's no room above.
static GRect prv_place_bubble(GRect anchor, GSize sz, GRect bounds) {
  const int cx = anchor.origin.x + anchor.size.w / 2;
  const int cy = anchor.origin.y + anchor.size.h / 2;

  GRect above = GRect(cx - sz.w / 2, anchor.origin.y - BUBBLE_LIFT - sz.h,
                      sz.w, sz.h);
  GRect right = GRect(anchor.origin.x + anchor.size.w + BUBBLE_LIFT,
                      cy - sz.h / 2, sz.w, sz.h);
  GRect left = GRect(anchor.origin.x - BUBBLE_LIFT - sz.w, cy - sz.h / 2,
                     sz.w, sz.h);
  GRect r = above;
  if (prv_fit_bubble(&r, bounds, anchor.origin.y + BUBBLE_OVERLAP - sz.h)) {
    return r;
  }
  r = right;
  if (prv_fit_bubble(&r, bounds, right.origin.y + sz.h)) {
    return r;
  }
  r = left;
  if (prv_fit_bubble(&r, bounds, left.origin.y + sz.h)) {
    return r;
  }
  return above; // nothing fits cleanly; let it clip
}

// Fills a bubble with a black outline, which separates it from whatever it
// overlaps (the white display band or neighbouring keys of the same colour).
static void prv_fill_bubble(GContext *ctx, GRect r, GColor bg) {
  graphics_context_set_fill_color(ctx, COLOR_BG);
  graphics_fill_rect(ctx, grect_inset(r, GEdgeInsets(-BUBBLE_BORDER)),
                     10 + BUBBLE_BORDER, GCornersAll);
  graphics_context_set_fill_color(ctx, bg);
  graphics_fill_rect(ctx, r, 10, GCornersAll);
}

// The press-preview bubble: an unoccluded copy of the key under the finger,
// drawn above it (or beside it, for keys with no room above) so you can see
// what liftoff will commit — the key itself is hidden under your fingertip.
static void prv_draw_key_bubble(GContext *ctx, GRect bounds) {
  const CalcButton *btn = calc_buttons_get(s_engine->page, s_pressed_index);
  if (!btn || btn->style == BUTTON_STYLE_NONE)
    return;

  GRect key = calc_buttons_get_button_draw_rect(btn);
  GRect r = prv_place_bubble(key, GSize(BUBBLE_W, BUBBLE_H), bounds);

  GColor bg, text;
  prv_get_button_colors(btn, false, s_engine->second_active, &bg, &text);
  prv_fill_bubble(ctx, r, bg);
  prv_draw_button_content(ctx, btn, s_pressed_index, r, bg, text,
                          s_engine->second_active);
}

// The page-flip hint: an armed swipe slides the target page's keypad in over
// the current one from the side it enters by (the right, for the next page),
// its leading edge tracking the finger — the content moves with the swipe,
// rather than an arrow pointing somewhere. The display stays put.
static void prv_draw_page_hint(GContext *ctx, GRect bounds) {
  const int top = calc_buttons_get_display_band_height();
  const int w = bounds.size.w;
  const int h = bounds.size.h - top;
  int travel = s_hint_travel < w ? s_hint_travel : w;

  bool next = s_hint == CALC_HINT_PAGE_NEXT;
  int target = (s_engine->page + (next ? 1 : CALC_PAGE_COUNT - 1)) %
               CALC_PAGE_COUNT;
  // The incoming keypad's left edge sits at edge_x - (next ? 0 : w).
  int edge_x = next ? w - travel : travel;
  GRect strip = next ? GRect(edge_x, top, travel, h) : GRect(0, top, travel, h);
  graphics_context_set_fill_color(ctx, COLOR_BG);
  graphics_fill_rect(ctx, strip, 0, GCornerNone);
  // Paging clears the 2nd modifier, so show the target page without it.
  prv_draw_buttons(ctx, target, next ? edge_x : edge_x - w, top, false);

  graphics_context_set_fill_color(ctx, COLOR_HINT_EDGE);
  graphics_fill_rect(ctx, GRect(edge_x - HINT_EDGE_W / 2, top, HINT_EDGE_W, h),
                     0, GCornerNone);
}

static void prv_draw_bubble(GContext *ctx, GRect bounds) {
  if (!s_engine)
    return;
  if (s_hint != CALC_HINT_NONE) {
    prv_draw_page_hint(ctx, bounds);
  } else if (s_pressed_index >= 0) {
    prv_draw_key_bubble(ctx, bounds);
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
  if (s_engine) {
    prv_draw_buttons(ctx, s_engine->page, 0, 0, s_engine->second_active);
  }

  // Press preview floats over everything else.
  prv_draw_bubble(ctx, bounds);
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

void calc_ui_set_hint(CalcGestureHint hint, int travel) {
  s_hint = hint;
  s_hint_travel = travel;
}

void calc_ui_set_engine(CalcEngine *engine) { s_engine = engine; }

void calc_ui_mark_dirty(void) {
  if (s_ui_layer) {
    layer_mark_dirty(s_ui_layer);
  }
}
