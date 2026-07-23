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
#define PERSIST_KEY_SWIPE_PAGING    8
#define PERSIST_KEY_ACTION_UP       9
#define PERSIST_KEY_ACTION_SELECT   10
#define PERSIST_KEY_ACTION_DOWN     11

// Curated set of functions assignable to the three physical buttons (UP,
// SELECT, DOWN) via Clay config. These integer values are PERSISTED and sent
// over AppMessage, so they must NEVER be reordered or reused — append only.
// They are deliberately decoupled from the CalcAction enum's ordering; the
// mapping to CalcAction lives in prv_resolve_keyfunc() in calc_settings.c and
// is mirrored by the select options in src/pkjs/config.js.
typedef enum {
  KEYFUNC_NONE = 0,       // disabled
  KEYFUNC_PAGE_NEXT = 1,
  KEYFUNC_PAGE_PREV = 2,
  KEYFUNC_BACKSPACE = 3,
  KEYFUNC_CLEAR = 4,
  KEYFUNC_NEGATE = 5,
  KEYFUNC_EQUALS = 6,
  KEYFUNC_ENTER = 7,      // RPN push
  KEYFUNC_SWAP = 8,       // X <-> Y
  KEYFUNC_ROLL_DOWN = 9,
  KEYFUNC_ROLL_UP = 10,
  KEYFUNC_DROP = 11,
  KEYFUNC_LAST_X = 12,
  KEYFUNC_M_RECALL = 13,
  KEYFUNC_M_PLUS = 14,
  KEYFUNC_M_MINUS = 15,
  KEYFUNC_M_CLEAR = 16,
  KEYFUNC_DRG_TOGGLE = 17,
  KEYFUNC_2ND_TOGGLE = 18,
  KEYFUNC_PI = 19,
  KEYFUNC_E = 20,
} KeyFunc;

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
bool calc_settings_swipe_paging_enabled(void);

// Resolved CalcAction for each physical button (KEYFUNC_NONE -> CALC_ACTION_NOOP).
CalcAction calc_settings_key_up_action(void);
CalcAction calc_settings_key_select_action(void);
CalcAction calc_settings_key_down_action(void);
