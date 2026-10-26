#include "calc_buttons.h"
#include "calc_engine.h"
#include "calc_fonts.h"
#include "calc_settings.h"
#include "calc_ui.h"
#include <pebble.h>

// ---------------------------------------------------------------------------
// Static state
// ---------------------------------------------------------------------------

static Window *s_window;
static Layer *s_ui_layer;
static CalcEngine s_engine;

// Touch gesture state. Each gesture locks into a mode early on:
//  - PENDING: just touched down. Too soon to tell a press from a flick, so
//    the touched key is previewed and a liftoff now is a plain tap.
//  - KEYS: pressing a key. The target follows the finger (slide-to-correct,
//    like the iOS keyboard) but only among the touched key's neighbours, since
//    real mistaps are off by one. Straying further turns a horizontal drag
//    into a swipe and anything else into a cancel.
//  - SWIPE: a page flip, entered by a fast horizontal start or a long
//    horizontal drag. Pulling back from the furthest point reached disarms it
//    (see SWIPE_RETREAT_PX).
// The UI always shows what liftoff will do — the key's preview bubble, the
// page to flip to, or nothing for a cancel — and liftoff does exactly that.
typedef enum {
  GESTURE_NONE,
  GESTURE_PENDING,
  GESTURE_KEYS,
  GESTURE_SWIPE,
} GestureMode;

static GestureMode s_gesture = GESTURE_NONE;
static int s_touch_button = -1;   // key liftoff would fire (-1 if none)
static CalcGestureHint s_touch_hint = CALC_HINT_NONE; // shown in its place
static int s_swipe_dir;           // armed swipe's direction: -1 left, 1 right
static int s_swipe_peak;          // furthest travel (px) in s_swipe_dir
static bool s_swipe_disarmed;     // pulled back out of a swipe; see
                                  // prv_update_keys
static GRect s_touch_home;        // touched key's bounds (a key-sized box
                                  // around touchdown if it missed every key);
                                  // KEYS mode retargets only near this
static GPoint s_touch_origin;     // touchdown point
static GPoint s_touch_last;       // last reported point (liftoff coords can be
                                  // stale on some drivers, so track it ourselves)
static uint32_t s_touch_down_ms;  // wall-clock ms of touchdown, for onset timing
static GRect s_screen_bounds;     // for edge-cancel detection

// Fingers tend to land a little below the spot you're aiming at, so hit-testing
// shifts each touch up by this much (negative shifts down). Experimental:
// measured error varies by screen region (the bottom-left reads low, the
// bottom-right reads high), so a single offset doesn't help — left at 0.
#define TOUCH_Y_OFFSET_PX 0

// Temporary: log raw touch coordinates (`pebble logs`) to work out where taps
// actually land relative to the keys.
#define TOUCH_DEBUG_LOG 1
#if TOUCH_DEBUG_LOG
static int s_debug_updates; // position updates seen in the current gesture
#endif

// Extra margin around the current key that still counts as "on" it while
// dragging, so a wobble at a shared cell edge doesn't flicker between keys.
#define TOUCH_SLOP_PX 4

// Onset lock: once the finger has travelled ONSET_PX from touchdown, a gesture
// that got there within ONSET_MS with horizontal travel dominating vertical by
// SWIPE_DOMINANCE_NUM/DEN is a flick (SWIPE); anything slower or more vertical
// is a press (KEYS). A finger still within ONSET_PX after ONSET_MS is a press.
#define ONSET_PX 10
#define ONSET_MS 120
#define SWIPE_DOMINANCE_NUM 3
#define SWIPE_DOMINANCE_DEN 2

// An armed SWIPE disarms (back to KEYS) once the finger pulls back
// SWIPE_RETREAT_PX from the furthest point it reached, or back past where it
// touched down. Measuring from the peak makes pulling back feel the same
// however far the swipe went; the margin rides out liftoff wobble.
#define SWIPE_RETREAT_PX 25

// Dragging a finger to within EDGE_CANCEL_PX of the screen edge (i.e. off the
// screen) cancels the press. The bottom-row keys run right up to the edge, so
// this only applies when the gesture began at least EDGE_TRAVEL_PX further
// in: a tap on + at the very bottom must still register.
#define EDGE_CANCEL_PX 6
#define EDGE_TRAVEL_PX 10

// Milliseconds since the epoch, composed into one monotonically increasing
// counter (time_ms's own return value only spans a single second).
static uint32_t prv_now_ms(void) {
  time_t s;
  uint16_t ms;
  time_ms(&s, &ms);
  return (uint32_t)s * 1000u + ms;
}

static void prv_touch_cancel(void) {
  s_gesture = GESTURE_NONE;
  s_touch_button = -1;
  s_touch_hint = CALC_HINT_NONE;
  s_swipe_disarmed = false;
  calc_ui_set_pressed(-1);
  calc_ui_set_hint(CALC_HINT_NONE);
}

static const uint32_t s_vibe_durations[] = {10};
static const VibePattern s_vibe_pattern = {.durations = s_vibe_durations,
                                           .num_segments = 1};

// ---------------------------------------------------------------------------
// Touch handling
// ---------------------------------------------------------------------------

// Whether p lies within margin px of the screen edge (the bezel, on round).
// Squared distances keep libm out of it.
static bool prv_near_edge(GPoint p, int margin) {
  const int w = s_screen_bounds.size.w;
  const int h = s_screen_bounds.size.h;
#if defined(PBL_ROUND)
  const int r = w / 2 - margin;
  const int dx = p.x - w / 2;
  const int dy = p.y - h / 2;
  return r <= 0 || dx * dx + dy * dy >= r * r;
#else
  return p.x < margin || p.y < margin || p.x >= w - margin ||
         p.y >= h - margin;
#endif
}

// A press is cancelled by dragging it off the screen: the finger is now at the
// edge, having started clearly inside it.
static bool prv_dragged_off_edge(void) {
  return prv_near_edge(s_touch_last, EDGE_CANCEL_PX) &&
         !prv_near_edge(s_touch_origin, EDGE_CANCEL_PX + EDGE_TRAVEL_PX);
}

// The point hit-testing should use for a raw touch (see TOUCH_Y_OFFSET_PX),
// clamped onto the screen so an adjusted touch at the very edge still lands
// on the edge key instead of falling off it.
static GPoint prv_hit_point(GPoint p) {
  int y = p.y - TOUCH_Y_OFFSET_PX;
  int max_y = s_screen_bounds.size.h - 1;
  return GPoint(p.x, y < 0 ? 0 : (y > max_y ? max_y : y));
}

// Whether the finger's travel since touchdown is mostly horizontal.
static bool prv_travel_horizontal(int dx, int dy) {
  int adx = dx < 0 ? -dx : dx;
  int ady = dy < 0 ? -dy : dy;
  return adx * SWIPE_DOMINANCE_DEN >= ady * SWIPE_DOMINANCE_NUM;
}

// Point the gesture at key idx (-1 for none) with the given hint, and redraw
// if that changes what's shown.
static void prv_set_target(int idx, CalcGestureHint hint) {
  if (idx == s_touch_button && hint == s_touch_hint) {
    return;
  }
  s_touch_button = idx;
  s_touch_hint = hint;
  calc_ui_set_pressed(idx);
  calc_ui_set_hint(hint);
  calc_ui_mark_dirty();
}

// Arm a page-flip swipe in the direction of dx.
static void prv_arm_swipe(int dx) {
  s_gesture = GESTURE_SWIPE;
  s_swipe_dir = dx < 0 ? -1 : 1;
  s_swipe_peak = dx * s_swipe_dir;
  prv_set_target(-1, dx < 0 ? CALC_HINT_PAGE_NEXT : CALC_HINT_PAGE_PREV);
}

// KEYS mode: retarget to the key under the finger, within the touched key's
// neighbourhood. Leaving the neighbourhood horizontally becomes a SWIPE (when
// swipe paging is on); leaving it any other way, dragging off the screen, or
// sitting over no key at all (the display, an empty slot) is a cancel. Once a
// swipe has been pulled back out of, leaving the neighbourhood stays a cancel
// until the finger returns to it — otherwise a finger still out there would
// re-arm straight away.
static void prv_update_keys(GPoint p) {
  int dx = p.x - s_touch_origin.x;
  int dy = p.y - s_touch_origin.y;
  GPoint hp = prv_hit_point(p);
  GRect hood = grect_inset(s_touch_home, GEdgeInsets(-(CALC_CELL_H + TOUCH_SLOP_PX),
                                                     -(CALC_CELL_W + TOUCH_SLOP_PX)));
  if (!grect_contains_point(&hood, &hp)) {
    if (!s_swipe_disarmed && calc_settings_swipe_paging_enabled() &&
        prv_travel_horizontal(dx, dy)) {
      prv_arm_swipe(dx);
    } else {
      prv_set_target(-1, CALC_HINT_NONE);
    }
    return;
  }
  s_swipe_disarmed = false;
  if (prv_dragged_off_edge()) {
    prv_set_target(-1, CALC_HINT_NONE);
    return;
  }
  // A little stickiness so the current key holds until the finger clearly
  // leaves it.
  const CalcButton *cur = calc_buttons_get(s_engine.page, s_touch_button);
  if (cur) {
    GRect slop = grect_inset(cur->bounds, GEdgeInsets(-TOUCH_SLOP_PX));
    if (grect_contains_point(&slop, &hp)) {
      prv_set_target(s_touch_button, CALC_HINT_NONE);
      return;
    }
  }
  prv_set_target(calc_buttons_hit_test(s_engine.page, hp, s_engine.rpn_mode),
                 CALC_HINT_NONE);
}

static void prv_touch_handler(const TouchEvent *event, void *context) {
  switch (event->type) {
  case TouchEvent_Touchdown: {
    s_touch_origin = GPoint(event->x, event->y);
    s_touch_last = s_touch_origin;
    s_touch_down_ms = prv_now_ms();
    s_gesture = GESTURE_PENDING;
    int idx = calc_buttons_hit_test(
        s_engine.page, prv_hit_point(s_touch_origin), s_engine.rpn_mode);
    const CalcButton *btn = calc_buttons_get(s_engine.page, idx);
    s_touch_home = btn ? btn->bounds
                       : GRect(s_touch_origin.x - CALC_CELL_W / 2,
                               s_touch_origin.y - CALC_CELL_H / 2, CALC_CELL_W,
                               CALC_CELL_H);
    // Touching no key (the display) is a cancel from the outset, but stays a
    // swipe candidate.
    prv_set_target(idx, CALC_HINT_NONE);
#if TOUCH_DEBUG_LOG
    APP_LOG(APP_LOG_LEVEL_INFO, "touch down (%d,%d) key=%d", event->x,
            event->y, s_touch_button);
    s_debug_updates = 0;
#endif
    break;
  }

  case TouchEvent_PositionUpdate: {
    if (s_gesture == GESTURE_NONE) {
      break;
    }
    GPoint p = GPoint(event->x, event->y);
    s_touch_last = p;
    int dx = p.x - s_touch_origin.x;
    int dy = p.y - s_touch_origin.y;
#if TOUCH_DEBUG_LOG
    ++s_debug_updates;
    APP_LOG(APP_LOG_LEVEL_INFO, "  move (%d,%d) mode=%d t=%lu", p.x, p.y,
            s_gesture, (unsigned long)(prv_now_ms() - s_touch_down_ms));
#endif
    if (s_gesture == GESTURE_PENDING) {
      uint32_t elapsed = prv_now_ms() - s_touch_down_ms;
      bool travelled = dx * dx + dy * dy >= ONSET_PX * ONSET_PX;
      if (travelled && elapsed <= ONSET_MS &&
          calc_settings_swipe_paging_enabled() &&
          prv_travel_horizontal(dx, dy)) {
        prv_arm_swipe(dx);
        break;
      } else if (travelled || elapsed > ONSET_MS) {
        s_gesture = GESTURE_KEYS;
      } else {
        break; // still deciding; keep previewing the touched key
      }
    }
    if (s_gesture == GESTURE_SWIPE) {
      int progress = dx * s_swipe_dir;
      if (progress > s_swipe_peak) {
        s_swipe_peak = progress;
      }
      if (progress > 0 && progress > s_swipe_peak - SWIPE_RETREAT_PX) {
        break; // still armed
      }
      s_gesture = GESTURE_KEYS;
      s_swipe_disarmed = true;
    }
    prv_update_keys(p);
    break;
  }

  case TouchEvent_Liftoff: {
#if TOUCH_DEBUG_LOG
    APP_LOG(APP_LOG_LEVEL_INFO,
            "touch up (%d,%d) last=(%d,%d) key=%d mode=%d moves=%d",
            event->x, event->y, s_touch_last.x, s_touch_last.y,
            s_touch_button, s_gesture, s_debug_updates);
#endif
    if (s_gesture == GESTURE_SWIPE) {
      // Swipe left (finger travels right→left) = next page, matching paged
      // scroll views elsewhere; swipe right = previous.
      calc_engine_handle_action(&s_engine, s_swipe_dir < 0
                                               ? CALC_ACTION_PAGE_NEXT
                                               : CALC_ACTION_PAGE_PREV);
    } else if (s_gesture != GESTURE_NONE) {
      const CalcButton *btn = calc_buttons_get(s_engine.page, s_touch_button);
      if (btn) {
        // Buzz only once a press actually registers, so sliding or flicking
        // across keys stays quiet.
        if (calc_settings_haptic_enabled()) {
          vibes_enqueue_custom_pattern(s_vibe_pattern);
        }
        CalcAction action = calc_button_get_action(btn, s_engine.rpn_mode);
        // The engine resolves the 2nd-modifier itself, so dispatch raw.
        calc_engine_handle_action(&s_engine, action);
      }
    }
    if (s_gesture != GESTURE_NONE) {
      prv_touch_cancel();
      calc_ui_mark_dirty();
    }
    break;
  }
  }
}

// ---------------------------------------------------------------------------
// Physical button handlers (shortcuts)
// ---------------------------------------------------------------------------

// The three physical buttons (UP/SELECT/DOWN) are user-configurable via Clay;
// each dispatches whatever CalcAction the settings resolve to. BACK is left
// untouched (default: exit app).
static void prv_do_key_action(CalcAction action) {
  if (action == CALC_ACTION_NOOP) {
    return;
  }
  // Page changes must cancel any in-flight touch: its origin index refers to
  // the OLD page's button table, so liftoff must not fire on the new page.
  if (action == CALC_ACTION_PAGE_NEXT || action == CALC_ACTION_PAGE_PREV) {
    prv_touch_cancel();
  }
  calc_engine_handle_action(&s_engine, action);
  calc_ui_mark_dirty();
}

static void prv_select_click(ClickRecognizerRef recognizer, void *context) {
  prv_do_key_action(calc_settings_key_select_action());
}

static void prv_up_click(ClickRecognizerRef recognizer, void *context) {
  prv_do_key_action(calc_settings_key_up_action());
}

static void prv_down_click(ClickRecognizerRef recognizer, void *context) {
  prv_do_key_action(calc_settings_key_down_action());
}

static void prv_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click);
  window_single_click_subscribe(BUTTON_ID_UP, prv_up_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_down_click);
}

// ---------------------------------------------------------------------------
// AppMessage — receive Clay config
// ---------------------------------------------------------------------------

static void prv_inbox_received(DictionaryIterator *iter, void *context) {
  calc_settings_handle_inbox(iter, &s_engine);
  calc_ui_mark_dirty();
}

// ---------------------------------------------------------------------------
// Window handlers
// ---------------------------------------------------------------------------

static void prv_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  s_screen_bounds = bounds;

  calc_buttons_init();
  calc_fonts_init();
  calc_engine_init(&s_engine);
  calc_settings_load(&s_engine);

  s_ui_layer = calc_ui_create(bounds);
  calc_ui_set_engine(&s_engine);
  layer_add_child(root, s_ui_layer);

  if (touch_service_is_enabled()) {
    touch_service_subscribe(prv_touch_handler, NULL);
  }
}

static void prv_window_unload(Window *window) {
  calc_settings_save(&s_engine);
  light_enable(false);

  touch_service_unsubscribe();
  calc_ui_destroy(s_ui_layer);
  s_ui_layer = NULL;
}

// ---------------------------------------------------------------------------
// App lifecycle
// ---------------------------------------------------------------------------

static void prv_init(void) {
  app_message_register_inbox_received(prv_inbox_received);
  app_message_open(128, 64);

  s_window = window_create();
  window_set_background_color(s_window, GColorWhite);
  window_set_click_config_provider(s_window, prv_click_config_provider);
  window_set_window_handlers(s_window, (WindowHandlers){
                                           .load = prv_window_load,
                                           .unload = prv_window_unload,
                                       });
  window_stack_push(s_window, true);
}

static void prv_deinit(void) { window_destroy(s_window); }

int main(void) {
  prv_init();
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Calculator initialized");
  app_event_loop();
  prv_deinit();
}
