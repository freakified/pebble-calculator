#include "calc_fonts.h"

static CalcFonts s_fonts;

void calc_fonts_init(void) {
  s_fonts.small = fonts_get_system_font(FONT_KEY_GOTHIC_18);
  s_fonts.large = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
  s_fonts.label = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  s_fonts.label_small = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
}

const CalcFonts *calc_fonts_get(void) { return &s_fonts; }
