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

// Touch gesture state. The target key follows the finger (slide-to-correct,
// like the iOS keyboard): whichever key is under the finger at liftoff fires,
// and the UI previews it in a bubble above the finger so you can see it.
// Lifting off over the display or an empty slot, or after dragging off the
// edge of the screen, cancels.
static int s_touch_button = -1;   // key currently under the finger (-1 if none)
static bool s_gesture_active = false;  // a touch gesture is in flight; tracked
                                       // even off-button so swipes work anywhere
static GPoint s_touch_origin;     // touchdown point, for swipe detection
static GPoint s_touch_last;       // last reported point (liftoff coords can be
                                  // stale on some drivers, so track it ourselves)
static uint32_t s_last_motion_ms; // wall-clock ms of the last update that showed
                                  // real motion; a stale value at liftoff means
                                  // the finger dwelled, which suppresses the swipe
static GRect s_screen_bounds;     // for edge-cancel detection
static GPoint s_motion_ref;       // point at which s_last_motion_ms was stamped;
                                  // motion is measured against this, not the last
                                  // update, so a slow continuous drift still stamps

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

// Horizontal liftoff displacement (px) that reclassifies a gesture as a
// left/right page-flip swipe instead of a key press. It's ~1.5 key widths so a
// one-key sideways correction can't flip the page. To avoid diagonal drags
// flipping pages, the horizontal travel must also dominate the vertical by
// SWIPE_DOMINANCE_NUM/DEN.
#define SWIPE_THRESHOLD_PX 75
#define SWIPE_DOMINANCE_NUM 3
#define SWIPE_DOMINANCE_DEN 2

// A swipe must end in motion: if the finger dwelled (stopped moving) for longer
// than SWIPE_DWELL_MS before liftoff, it's a press rather than a page flip —
// you slid to a key, paused to check the preview, and lifted. Jitter under
// MOTION_EPS_PX doesn't count as motion, so holding still truly dwells.
#define SWIPE_DWELL_MS 250
#define MOTION_EPS_PX 3

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
  s_gesture_active = false;
  if (s_touch_button >= 0) {
    s_touch_button = -1;
    calc_ui_set_pressed(-1);
  }
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

// Make idx the key under the finger.
static void prv_set_touch_button(int idx) {
  if (idx == s_touch_button) {
    return;
  }
  s_touch_button = idx;
  calc_ui_set_pressed(idx);
  calc_ui_mark_dirty();
}

static void prv_touch_handler(const TouchEvent *event, void *context) {
  switch (event->type) {
  case TouchEvent_Touchdown: {
    s_touch_origin = GPoint(event->x, event->y);
    s_touch_last = s_touch_origin;
    s_motion_ref = s_touch_origin;
    s_last_motion_ms = prv_now_ms();
    s_gesture_active = true;
    s_touch_button = -1;
    prv_set_touch_button(calc_buttons_hit_test(
        s_engine.page, prv_hit_point(s_touch_origin), s_engine.rpn_mode));
#if TOUCH_DEBUG_LOG
    APP_LOG(APP_LOG_LEVEL_INFO, "touch down (%d,%d) key=%d", event->x,
            event->y, s_touch_button);
    s_debug_updates = 0;
#endif
    break;
  }

  case TouchEvent_PositionUpdate: {
    if (!s_gesture_active) {
      break;
    }
    GPoint p = GPoint(event->x, event->y);
    s_touch_last = p;
#if TOUCH_DEBUG_LOG
    ++s_debug_updates;
    APP_LOG(APP_LOG_LEVEL_INFO, "  move (%d,%d) key=%d", p.x, p.y,
            calc_buttons_hit_test(s_engine.page, prv_hit_point(p),
                                  s_engine.rpn_mode));
#endif
    // Re-stamp the motion clock only when the finger has actually travelled past
    // MOTION_EPS_PX from where it was last stamped. Measuring against s_motion_ref
    // (not the previous update) means a slow drag of many tiny steps still trips
    // the threshold cumulatively, while a held finger's jitter never does — so
    // the dwell timer at liftoff reflects real stillness.
    int mdx = p.x - s_motion_ref.x;
    int mdy = p.y - s_motion_ref.y;
    if (mdx * mdx + mdy * mdy >= MOTION_EPS_PX * MOTION_EPS_PX) {
      s_motion_ref = p;
      s_last_motion_ms = prv_now_ms();
    }
    // Off the edge of the screen: nothing will fire, so drop the highlight and
    // bubble to show it. Sliding back in picks up a key again.
    if (prv_dragged_off_edge()) {
      prv_set_touch_button(-1);
      break;
    }
    // Retarget to whatever key is now under the finger, with a little
    // stickiness so the current key holds until the finger clearly leaves it.
    GPoint hp = prv_hit_point(p);
    const CalcButton *cur = calc_buttons_get(s_engine.page, s_touch_button);
    if (cur) {
      GRect slop = grect_inset(cur->bounds, GEdgeInsets(-TOUCH_SLOP_PX));
      if (grect_contains_point(&slop, &hp)) {
        break;
      }
    }
    prv_set_touch_button(
        calc_buttons_hit_test(s_engine.page, hp, s_engine.rpn_mode));
    break;
  }

  case TouchEvent_Liftoff: {
#if TOUCH_DEBUG_LOG
    APP_LOG(APP_LOG_LEVEL_INFO,
            "touch up (%d,%d) last=(%d,%d) key=%d edge=%d moves=%d",
            event->x, event->y, s_touch_last.x, s_touch_last.y,
            s_touch_button, prv_dragged_off_edge(), s_debug_updates);
#endif
    if (s_gesture_active) {
      // A fast horizontal flick flips pages, wherever it started; otherwise
      // the key under the finger (if any) fires.
      int dx = s_touch_last.x - s_touch_origin.x;
      int dy = s_touch_last.y - s_touch_origin.y;
      int adx = dx < 0 ? -dx : dx;
      int ady = dy < 0 ? -dy : dy;
      // A swipe must still be in motion at liftoff. If the finger dwelled past
      // SWIPE_DWELL_MS, it settled on a key on purpose: commit that instead.
      bool moving = (prv_now_ms() - s_last_motion_ms) <= SWIPE_DWELL_MS;
      bool is_swipe = calc_settings_swipe_paging_enabled() && moving &&
                      adx >= SWIPE_THRESHOLD_PX &&
                      adx * SWIPE_DOMINANCE_DEN >= ady * SWIPE_DOMINANCE_NUM;
      if (is_swipe) {
        // Swipe left (finger travels right→left) = next page, matching paged
        // scroll views elsewhere; swipe right = previous.
        calc_engine_handle_action(
            &s_engine, dx < 0 ? CALC_ACTION_PAGE_NEXT : CALC_ACTION_PAGE_PREV);
      } else if (!prv_dragged_off_edge()) {
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
      s_gesture_active = false;
      s_touch_button = -1;
      calc_ui_set_pressed(-1);
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
