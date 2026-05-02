#pragma once

#include <pebble.h>

typedef enum {
  CALC_ICON_NONE = 0,
  CALC_ICON_PLUS,
  CALC_ICON_MINUS,
  CALC_ICON_MULTIPLY,
  CALC_ICON_DIVIDE,
  CALC_ICON_BACKSPACE,
} CalcIcon;

void calc_icons_draw(GContext *ctx, CalcIcon icon, GRect rect, GColor fg,
                     GColor bg);
