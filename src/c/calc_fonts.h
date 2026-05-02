#pragma once

#include <pebble.h>

typedef struct {
  GFont x_register;
  GFont y_register;
  GFont indicator;
  GFont button_num;
  GFont button_label;
  GFont button_label_small;
} CalcFonts;

// Initialize fonts (should be called during app/window load)
void calc_fonts_init(void);

// Get the centralized fonts struct
const CalcFonts* calc_fonts_get(void);
