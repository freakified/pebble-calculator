#include "calc_icons.h"

#define ICON_STROKE_WIDTH 2
#define ICON_ARM_HALF 7
#define ICON_DIVIDE_DOT_RADIUS 2
#define ICON_DIVIDE_DOT_OFFSET 6
#define ICON_EQUALS_HALF_H 3

#define BKSP_LEFT (-12)
#define BKSP_RIGHT 14
#define BKSP_RECT_LEFT (-4)
#define BKSP_HALF_H 8
#define BKSP_X_LEFT 0
#define BKSP_X_RIGHT 8
#define BKSP_X_HALF_H 4

static GPath *s_backspace_path = NULL;

static GPathInfo s_backspace_path_info = {
    .num_points = 5,
    .points =
        (GPoint[]){
            {BKSP_LEFT, 0},
            {BKSP_RECT_LEFT, -BKSP_HALF_H},
            {BKSP_RIGHT, -BKSP_HALF_H},
            {BKSP_RIGHT, BKSP_HALF_H},
            {BKSP_RECT_LEFT, BKSP_HALF_H},
        },
};

static void prv_draw_plus(GContext *ctx, GPoint c, GColor fg) {
  graphics_context_set_stroke_color(ctx, fg);
  graphics_context_set_stroke_width(ctx, ICON_STROKE_WIDTH);
  graphics_draw_line(ctx, GPoint(c.x - ICON_ARM_HALF, c.y),
                     GPoint(c.x + ICON_ARM_HALF, c.y));
  graphics_draw_line(ctx, GPoint(c.x, c.y - ICON_ARM_HALF),
                     GPoint(c.x, c.y + ICON_ARM_HALF));
}

static void prv_draw_minus(GContext *ctx, GPoint c, GColor fg) {
  graphics_context_set_stroke_color(ctx, fg);
  graphics_context_set_stroke_width(ctx, ICON_STROKE_WIDTH);
  graphics_draw_line(ctx, GPoint(c.x - ICON_ARM_HALF, c.y),
                     GPoint(c.x + ICON_ARM_HALF, c.y));
}

static void prv_draw_multiply(GContext *ctx, GPoint c, GColor fg) {
  graphics_context_set_stroke_color(ctx, fg);
  graphics_context_set_stroke_width(ctx, ICON_STROKE_WIDTH);
  graphics_draw_line(ctx, GPoint(c.x - ICON_ARM_HALF, c.y - ICON_ARM_HALF),
                     GPoint(c.x + ICON_ARM_HALF, c.y + ICON_ARM_HALF));
  graphics_draw_line(ctx, GPoint(c.x - ICON_ARM_HALF, c.y + ICON_ARM_HALF),
                     GPoint(c.x + ICON_ARM_HALF, c.y - ICON_ARM_HALF));
}

static void prv_draw_divide(GContext *ctx, GPoint c, GColor fg) {
  graphics_context_set_stroke_color(ctx, fg);
  graphics_context_set_stroke_width(ctx, ICON_STROKE_WIDTH);
  graphics_draw_line(ctx, GPoint(c.x - ICON_ARM_HALF, c.y),
                     GPoint(c.x + ICON_ARM_HALF, c.y));
  graphics_context_set_fill_color(ctx, fg);
  graphics_fill_circle(ctx, GPoint(c.x, c.y - ICON_DIVIDE_DOT_OFFSET),
                       ICON_DIVIDE_DOT_RADIUS);
  graphics_fill_circle(ctx, GPoint(c.x, c.y + ICON_DIVIDE_DOT_OFFSET),
                       ICON_DIVIDE_DOT_RADIUS);
}

static void prv_draw_equals(GContext *ctx, GPoint c, GColor fg) {
  graphics_context_set_stroke_color(ctx, fg);
  graphics_context_set_stroke_width(ctx, ICON_STROKE_WIDTH);
  graphics_draw_line(ctx, GPoint(c.x - ICON_ARM_HALF, c.y - ICON_EQUALS_HALF_H),
                     GPoint(c.x + ICON_ARM_HALF, c.y - ICON_EQUALS_HALF_H));
  graphics_draw_line(ctx, GPoint(c.x - ICON_ARM_HALF, c.y + ICON_EQUALS_HALF_H),
                     GPoint(c.x + ICON_ARM_HALF, c.y + ICON_EQUALS_HALF_H));
}

static void prv_draw_backspace(GContext *ctx, GPoint c, GColor fg, GColor bg) {
  GPoint cc = GPoint(c.x - 2, c.y);
  if (!s_backspace_path) {
    s_backspace_path = gpath_create(&s_backspace_path_info);
  }
  gpath_move_to(s_backspace_path, cc);
  graphics_context_set_fill_color(ctx, fg);
  gpath_draw_filled(ctx, s_backspace_path);

  // Knock the × out of the rectangle portion using the button bg color.
  graphics_context_set_stroke_color(ctx, bg);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_line(ctx, GPoint(cc.x + BKSP_X_LEFT, cc.y - BKSP_X_HALF_H),
                     GPoint(cc.x + BKSP_X_RIGHT, cc.y + BKSP_X_HALF_H));
  graphics_draw_line(ctx, GPoint(cc.x + BKSP_X_LEFT, cc.y + BKSP_X_HALF_H),
                     GPoint(cc.x + BKSP_X_RIGHT, cc.y - BKSP_X_HALF_H));
}

void calc_icons_draw(GContext *ctx, CalcIcon icon, GRect rect, GColor fg,
                     GColor bg) {
  GPoint c =
      GPoint(rect.origin.x + rect.size.w / 2, rect.origin.y + rect.size.h / 2);

  switch (icon) {
  case CALC_ICON_NONE:
    return;
  case CALC_ICON_PLUS:
    prv_draw_plus(ctx, c, fg);
    return;
  case CALC_ICON_MINUS:
    prv_draw_minus(ctx, c, fg);
    return;
  case CALC_ICON_MULTIPLY:
    prv_draw_multiply(ctx, c, fg);
    return;
  case CALC_ICON_DIVIDE:
    prv_draw_divide(ctx, c, fg);
    return;
  case CALC_ICON_EQUALS:
    prv_draw_equals(ctx, c, fg);
    return;
  case CALC_ICON_BACKSPACE:
    prv_draw_backspace(ctx, c, fg, bg);
    return;
  }
}
