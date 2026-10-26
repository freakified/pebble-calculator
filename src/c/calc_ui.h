#pragma once

#include <pebble.h>
#include "calc_engine.h"

// Create the calculator UI layer (fills the entire window)
Layer *calc_ui_create(GRect bounds);

// Destroy the UI layer
void calc_ui_destroy(Layer *layer);

// Set which button is currently pressed (-1 = none). The pressed key is
// highlighted and previewed in a bubble above the finger.
void calc_ui_set_pressed(int button_index);

// What a touch gesture will do at liftoff when that isn't pressing a key.
typedef enum {
  CALC_HINT_NONE,      // no hint (the pressed key's preview, if any, shows)
  CALC_HINT_PAGE_NEXT, // liftoff flips to the next page
  CALC_HINT_PAGE_PREV, // liftoff flips to the previous page
} CalcGestureHint;

// Show a gesture hint over the keypad, in place of the key preview (so clear
// the pressed key alongside it). travel is how far (px) the finger has moved
// horizontally, which the page-flip hint tracks.
void calc_ui_set_hint(CalcGestureHint hint, int travel);

// Set reference to the engine (for reading display values)
void calc_ui_set_engine(CalcEngine *engine);

// Mark the UI as needing redraw
void calc_ui_mark_dirty(void);
