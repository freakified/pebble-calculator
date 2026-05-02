#pragma once

#include <pebble.h>
#include "calc_engine.h"

// Create the calculator UI layer (fills the entire window)
Layer *calc_ui_create(GRect bounds);

// Destroy the UI layer
void calc_ui_destroy(Layer *layer);

// Set which button is currently pressed (-1 = none)
void calc_ui_set_pressed(int button_index);

// Set reference to the engine (for reading display values)
void calc_ui_set_engine(CalcEngine *engine);

// Mark the UI as needing redraw
void calc_ui_mark_dirty(void);
