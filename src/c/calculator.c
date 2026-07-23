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

// Touch gesture state: the button under the initial touchdown stays the
// gesture's target for its whole lifetime, and stays visually pressed the whole
// time — even while the finger is dragged off to a cancel position (matching
// Apple's calculator, so you can confirm which key you hit before committing).
// Off the button it shows a muted "cancel" highlight instead of the committing
// one. s_touch_inside tracks whether liftoff would land on the button; only
// liftoff while inside fires the action.
static int s_touch_button = -1;   // origin button of the current gesture (-1 if
                                  // the gesture began off any button)
static bool s_touch_inside = false;
static bool s_gesture_active = false;  // a touch gesture is in flight; tracked
                                       // even off-button so swipes work anywhere
static GPoint s_touch_origin;     // touchdown point, for swipe detection
static GPoint s_touch_last;       // last reported point (liftoff coords can be
                                  // stale on some drivers, so track it ourselves)
static uint32_t s_last_motion_ms; // wall-clock ms of the last update that showed
                                  // real motion; a stale value at liftoff means
                                  // the finger dwelled, which suppresses the swipe
static GPoint s_motion_ref;       // point at which s_last_motion_ms was stamped;
                                  // motion is measured against this, not the last
                                  // update, so a slow continuous drift still stamps

// Extra margin around a button that still counts as "inside" while dragging,
// so a small wobble at a shared cell edge doesn't cancel the press.
#define TOUCH_SLOP_PX 4

// Horizontal liftoff displacement (px) that reclassifies a gesture as a
// left/right page-flip swipe instead of a (cancelled) button press. To avoid
// diagonal drags flipping pages, the horizontal travel must also dominate the
// vertical by SWIPE_DOMINANCE_NUM/DEN.
#define SWIPE_THRESHOLD_PX 30
#define SWIPE_DOMINANCE_NUM 3
#define SWIPE_DOMINANCE_DEN 2

// A swipe must end in motion: if the finger dwelled (stopped moving) for longer
// than SWIPE_DWELL_MS before liftoff, treat it as a deliberate cancel rather
// than a page flip — you dragged, paused, and lifted, which reads as "never
// mind" even though the displacement still clears SWIPE_THRESHOLD_PX. Jitter
// under MOTION_EPS_PX doesn't count as motion, so holding still truly dwells.
#define SWIPE_DWELL_MS 250
#define MOTION_EPS_PX 3

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
    s_touch_inside = false;
    calc_ui_set_pressed(-1, false);
  }
}

static const uint32_t s_vibe_durations[] = {10};
static const VibePattern s_vibe_pattern = {.durations = s_vibe_durations,
                                           .num_segments = 1};

// ---------------------------------------------------------------------------
// Touch handling
// ---------------------------------------------------------------------------

static void prv_touch_handler(const TouchEvent *event, void *context) {
  switch (event->type) {
  case TouchEvent_Touchdown: {
    s_touch_origin = GPoint(event->x, event->y);
    s_touch_last = s_touch_origin;
    s_motion_ref = s_touch_origin;
    s_last_motion_ms = prv_now_ms();
    s_gesture_active = true;
    int idx = calc_buttons_hit_test(s_engine.page, GPoint(event->x, event->y),
                                    s_engine.rpn_mode);
    if (idx >= 0) {
      s_touch_button = idx;
      s_touch_inside = true;
      calc_ui_set_pressed(idx, false);
      if (calc_settings_haptic_enabled()) {
        vibes_enqueue_custom_pattern(s_vibe_pattern);
      }
      calc_ui_mark_dirty();
    }
    break;
  }

  case TouchEvent_PositionUpdate: {
    if (!s_gesture_active) {
      break;
    }
    GPoint p = GPoint(event->x, event->y);
    s_touch_last = p;
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
    // Button highlight tracking only applies when the gesture began on a button;
    // an off-button drag is a swipe candidate and has nothing to highlight.
    if (s_touch_button >= 0) {
      const CalcButton *btn = calc_buttons_get(s_engine.page, s_touch_button);
      bool inside = false;
      if (btn) {
        GRect slop = grect_inset(btn->bounds, GEdgeInsets(-TOUCH_SLOP_PX));
        inside = grect_contains_point(&slop, &p);
      }
      // The button stays pressed either way; crossing the boundary only swaps
      // the committing highlight for the muted cancel highlight (and back).
      if (inside != s_touch_inside) {
        s_touch_inside = inside;
        calc_ui_set_pressed(s_touch_button, !inside);
        calc_ui_mark_dirty();
      }
    }
    break;
  }

  case TouchEvent_Liftoff: {
    if (s_gesture_active) {
      // A horizontal drag flips pages, wherever it started. When it began on a
      // button, that button was already cancelled during the drag (the travel
      // dwarfs TOUCH_SLOP_PX), so s_touch_inside is false and it never commits.
      int dx = s_touch_last.x - s_touch_origin.x;
      int dy = s_touch_last.y - s_touch_origin.y;
      int adx = dx < 0 ? -dx : dx;
      int ady = dy < 0 ? -dy : dy;
      // A swipe must still be in motion at liftoff. If the finger dwelled past
      // SWIPE_DWELL_MS, this is a cancel: is_swipe stays false, and because any
      // origin button was un-highlighted during the drag (s_touch_inside false),
      // liftoff commits nothing at all — exactly the touch-cancel outcome.
      bool moving = (prv_now_ms() - s_last_motion_ms) <= SWIPE_DWELL_MS;
      bool is_swipe = moving && adx >= SWIPE_THRESHOLD_PX &&
                      adx * SWIPE_DOMINANCE_DEN >= ady * SWIPE_DOMINANCE_NUM;
      if (is_swipe) {
        // Swipe left (finger travels right→left) = next page, matching paged
        // scroll views elsewhere; swipe right = previous.
        calc_engine_handle_action(
            &s_engine, dx < 0 ? CALC_ACTION_PAGE_NEXT : CALC_ACTION_PAGE_PREV);
      } else if (s_touch_button >= 0) {
        const CalcButton *btn = calc_buttons_get(s_engine.page, s_touch_button);
        if (btn && s_touch_inside) {
          CalcAction action = calc_button_get_action(btn, s_engine.rpn_mode);
          // The engine resolves the 2nd-modifier itself, so dispatch raw.
          calc_engine_handle_action(&s_engine, action);
        }
      }
      s_gesture_active = false;
      s_touch_button = -1;
      s_touch_inside = false;
      calc_ui_set_pressed(-1, false);
      calc_ui_mark_dirty();
    }
    break;
  }
  }
}

// ---------------------------------------------------------------------------
// Physical button handlers (shortcuts)
// ---------------------------------------------------------------------------

// SELECT cycles through pages — the on-screen ± has moved to the scientific
// page. Any in-flight touch gesture is cancelled: its origin index refers to
// the OLD page's button table, so liftoff must not fire on the new page.
static void prv_select_click(ClickRecognizerRef recognizer, void *context) {
  prv_touch_cancel();
  calc_engine_handle_action(&s_engine, CALC_ACTION_PAGE_NEXT);
  calc_ui_mark_dirty();
}

// UP: swap X↔Y in RPN; negate in standard (rect basic page has no ± key).
static void prv_up_click(ClickRecognizerRef recognizer, void *context) {
  calc_engine_handle_action(&s_engine, s_engine.rpn_mode ? CALC_ACTION_SWAP
                                                         : CALC_ACTION_NEGATE);
  calc_ui_mark_dirty();
}

static void prv_down_click(ClickRecognizerRef recognizer, void *context) {
  calc_engine_handle_action(&s_engine, CALC_ACTION_BACKSPACE);
  calc_ui_mark_dirty();
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
