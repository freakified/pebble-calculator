#pragma once

#include <pebble.h>

typedef struct {
  GFont small;
  GFont large;
  GFont label;
  GFont label_small;
} CalcFonts;

// Initialize fonts (should be called during app/window load)
void calc_fonts_init(void);

// Get the centralized fonts struct
const CalcFonts* calc_fonts_get(void);
