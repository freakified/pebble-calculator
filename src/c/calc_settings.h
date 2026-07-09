#pragma once

#include <pebble.h>
#include "calc_engine.h"

// Persistent storage keys. The current page is intentionally NOT persisted —
// the app always opens on the basic page so quick calculations don't require
// navigating back from leftover scientific/memory pages.
#define PERSIST_KEY_RPN_MODE        1
#define PERSIST_KEY_HAPTIC_FEEDBACK 2
#define PERSIST_KEY_MAIN_NUMBER     3
#define PERSIST_KEY_KEEP_BACKLIGHT  4
#define PERSIST_KEY_DEG_MODE        5
#define PERSIST_KEY_MEMORY          6
#define PERSIST_KEY_STACK           7  // double[4]: T, Z, Y, X

// Restore persisted state into the engine and the module-private prefs.
// Calls light_enable() if the keep-backlight pref was set.
void calc_settings_load(CalcEngine *engine);

// Persist transient state (main number, memory, RPN stack, DEG mode) at
// window unload. RPN/haptic/backlight are written eagerly by handle_inbox
// when changed; DEG is also saved here since the DRG key changes it on-watch.
void calc_settings_save(CalcEngine *engine);

// Apply incoming Clay config changes: update engine + prefs and persist.
void calc_settings_handle_inbox(DictionaryIterator *iter, CalcEngine *engine);

bool calc_settings_haptic_enabled(void);
bool calc_settings_keep_backlight(void);
