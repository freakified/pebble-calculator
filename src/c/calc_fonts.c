#include "calc_fonts.h"

static CalcFonts s_fonts;

void calc_fonts_init(void) {
  s_fonts.x_register = fonts_get_system_font(FONT_KEY_LECO_32_BOLD_NUMBERS);
  s_fonts.y_register = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  s_fonts.indicator = fonts_get_system_font(FONT_KEY_GOTHIC_18);
  s_fonts.button_num = fonts_get_system_font(FONT_KEY_LECO_32_BOLD_NUMBERS);
  s_fonts.button_label = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
  s_fonts.button_label_small = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
}

const CalcFonts *calc_fonts_get(void) { return &s_fonts; }
