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
// gesture's target for its whole lifetime. Dragging off it cancels the press
// visually, but dragging back re-arms it (matching platform button behavior);
// only liftoff while inside fires the action.
static int s_touch_button = -1;   // origin button of the current gesture
static bool s_touch_inside = false;

// Extra margin around a button that still counts as "inside" while dragging,
// so a small wobble at a shared cell edge doesn't cancel the press.
#define TOUCH_SLOP_PX 4

static const uint32_t s_vibe_durations[] = {10};
static const VibePattern s_vibe_pattern = {.durations = s_vibe_durations,
                                           .num_segments = 1};

// ---------------------------------------------------------------------------
// Touch handling
// ---------------------------------------------------------------------------

static void prv_touch_handler(const TouchEvent *event, void *context) {
  switch (event->type) {
  case TouchEvent_Touchdown: {
    int idx = calc_buttons_hit_test(s_engine.page, GPoint(event->x, event->y),
                                    s_engine.rpn_mode);
    if (idx >= 0) {
      s_touch_button = idx;
      s_touch_inside = true;
      calc_ui_set_pressed(idx);
      if (calc_settings_haptic_enabled()) {
        vibes_enqueue_custom_pattern(s_vibe_pattern);
      }
      calc_ui_mark_dirty();
    }
    break;
  }

  case TouchEvent_PositionUpdate: {
    if (s_touch_button >= 0) {
      const CalcButton *btn = calc_buttons_get(s_engine.page, s_touch_button);
      GPoint p = GPoint(event->x, event->y);
      bool inside = false;
      if (btn) {
        GRect slop = grect_inset(btn->bounds, GEdgeInsets(-TOUCH_SLOP_PX));
        inside = grect_contains_point(&slop, &p);
      }
      if (inside != s_touch_inside) {
        s_touch_inside = inside;
        calc_ui_set_pressed(inside ? s_touch_button : -1);
        calc_ui_mark_dirty();
      }
    }
    break;
  }

  case TouchEvent_Liftoff: {
    if (s_touch_button >= 0) {
      const CalcButton *btn = calc_buttons_get(s_engine.page, s_touch_button);
      if (btn && s_touch_inside) {
        CalcAction action = calc_button_get_action(btn, s_engine.rpn_mode);
        // The engine resolves the 2nd-modifier itself, so dispatch raw.
        calc_engine_handle_action(&s_engine, action);
      }
      s_touch_button = -1;
      s_touch_inside = false;
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

// SELECT cycles through pages — the on-screen ± has moved to the scientific
// page.
static void prv_select_click(ClickRecognizerRef recognizer, void *context) {
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
