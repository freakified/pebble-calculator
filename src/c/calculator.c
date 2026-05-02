#include <pebble.h>
#include "calc_engine.h"
#include "calc_buttons.h"
#include "calc_ui.h"
#include "calc_fonts.h"

// ---------------------------------------------------------------------------
// Persistent storage keys
// ---------------------------------------------------------------------------

#define PERSIST_KEY_RPN_MODE 1
#define PERSIST_KEY_HAPTIC_FEEDBACK 2

// ---------------------------------------------------------------------------
// Static state
// ---------------------------------------------------------------------------

#define CL_LONG_PRESS_MS 500

static Window *s_window;
static Layer *s_ui_layer;
static CalcEngine s_engine;
static int s_pressed_button = -1;
static bool s_haptic_feedback = true;
static AppTimer *s_cl_longpress_timer = NULL;
static bool s_cl_longpress_fired = false;

// ---------------------------------------------------------------------------
// Touch handling
// ---------------------------------------------------------------------------

static void prv_cancel_longpress_timer(void) {
  if (s_cl_longpress_timer) {
    app_timer_cancel(s_cl_longpress_timer);
    s_cl_longpress_timer = NULL;
  }
}

static void prv_cl_longpress_cb(void *context) {
  s_cl_longpress_timer = NULL;
  s_cl_longpress_fired = true;
  CalcAction action =
      s_engine.rpn_mode ? CALC_ACTION_DROP : CALC_ACTION_CLEAR;
  calc_engine_handle_action(&s_engine, action);
  if (s_haptic_feedback) {
    vibes_long_pulse();
  }
  calc_ui_mark_dirty();
}

static void prv_touch_handler(const TouchEvent *event, void *context) {
  switch (event->type) {
    case TouchEvent_Touchdown: {
      int idx = calc_buttons_hit_test(GPoint(event->x, event->y));
      if (idx >= 0) {
        s_pressed_button = idx;
        calc_ui_set_pressed(idx);
        if (s_haptic_feedback) {
          vibes_short_pulse();
        }
        if (idx == CALC_BUTTON_INDEX_CL) {
          s_cl_longpress_fired = false;
          s_cl_longpress_timer =
              app_timer_register(CL_LONG_PRESS_MS, prv_cl_longpress_cb, NULL);
        }
        calc_ui_mark_dirty();
      }
      break;
    }

    case TouchEvent_PositionUpdate: {
      if (s_pressed_button >= 0) {
        const CalcButton *btn = calc_buttons_get(s_pressed_button);
        GPoint p = GPoint(event->x, event->y);
        if (btn && !grect_contains_point(&btn->bounds, &p)) {
          // Finger moved off the button — cancel
          prv_cancel_longpress_timer();
          s_pressed_button = -1;
          calc_ui_set_pressed(-1);
          calc_ui_mark_dirty();
        }
      }
      break;
    }

    case TouchEvent_Liftoff: {
      if (s_pressed_button >= 0) {
        const CalcButton *btn = calc_buttons_get(s_pressed_button);
        prv_cancel_longpress_timer();
        if (btn && !s_cl_longpress_fired) {
          CalcAction action = calc_button_get_action(btn, s_engine.rpn_mode);
          calc_engine_handle_action(&s_engine, action);
        }
        s_cl_longpress_fired = false;
        s_pressed_button = -1;
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

static void prv_select_click(ClickRecognizerRef recognizer, void *context) {
  // Select = Equals (standard) or Enter (RPN)
  if (s_engine.rpn_mode) {
    calc_engine_handle_action(&s_engine, CALC_ACTION_ENTER);
  } else {
    calc_engine_handle_action(&s_engine, CALC_ACTION_EQUALS);
  }
  calc_ui_mark_dirty();
}

static void prv_up_click(ClickRecognizerRef recognizer, void *context) {
  // Up = SWAP (RPN) or Negate (standard)
  if (s_engine.rpn_mode) {
    calc_engine_handle_action(&s_engine, CALC_ACTION_SWAP);
  } else {
    calc_engine_handle_action(&s_engine, CALC_ACTION_NEGATE);
  }
  calc_ui_mark_dirty();
}

static void prv_down_click(ClickRecognizerRef recognizer, void *context) {
  // Down = Clear
  calc_engine_handle_action(&s_engine, CALC_ACTION_CLEAR);
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
  Tuple *rpn_tuple = dict_find(iter, MESSAGE_KEY_RPN_MODE);
  if (rpn_tuple) {
    bool rpn = rpn_tuple->value->int32 != 0;
    persist_write_bool(PERSIST_KEY_RPN_MODE, rpn);
    calc_engine_set_rpn_mode(&s_engine, rpn);
    calc_ui_mark_dirty();
    APP_LOG(APP_LOG_LEVEL_INFO, "RPN mode set to %d", rpn);
  }

  Tuple *haptic_tuple = dict_find(iter, MESSAGE_KEY_HAPTIC_FEEDBACK);
  if (haptic_tuple) {
    s_haptic_feedback = haptic_tuple->value->int32 != 0;
    persist_write_bool(PERSIST_KEY_HAPTIC_FEEDBACK, s_haptic_feedback);
    APP_LOG(APP_LOG_LEVEL_INFO, "Haptic feedback set to %d", s_haptic_feedback);
  }
}

// ---------------------------------------------------------------------------
// Window handlers
// ---------------------------------------------------------------------------

static void prv_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  // Initialize button layout
  calc_buttons_init();

  // Initialize fonts
  calc_fonts_init();

  // Initialize calculator engine
  calc_engine_init(&s_engine);

  // Restore RPN mode from persistent storage
  if (persist_exists(PERSIST_KEY_RPN_MODE)) {
    bool rpn = persist_read_bool(PERSIST_KEY_RPN_MODE);
    s_engine.rpn_mode = rpn;
  }

  // Restore haptic feedback setting
  if (persist_exists(PERSIST_KEY_HAPTIC_FEEDBACK)) {
    s_haptic_feedback = persist_read_bool(PERSIST_KEY_HAPTIC_FEEDBACK);
  }

  // Create UI
  s_ui_layer = calc_ui_create(bounds);
  calc_ui_set_engine(&s_engine);
  layer_add_child(root, s_ui_layer);

  // Subscribe to touch
  if (touch_service_is_enabled()) {
    touch_service_subscribe(prv_touch_handler, NULL);
  }
}

static void prv_window_unload(Window *window) {
  touch_service_unsubscribe();
  calc_ui_destroy(s_ui_layer);
  s_ui_layer = NULL;
}

// ---------------------------------------------------------------------------
// App lifecycle
// ---------------------------------------------------------------------------

static void prv_init(void) {
  // Open AppMessage for Clay config
  app_message_register_inbox_received(prv_inbox_received);
  app_message_open(128, 64);

  // Create window
  s_window = window_create();
  window_set_background_color(s_window, GColorWhite);
  window_set_click_config_provider(s_window, prv_click_config_provider);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  window_stack_push(s_window, true);
}

static void prv_deinit(void) {
  window_destroy(s_window);
}

int main(void) {
  prv_init();
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Calculator initialized");
  app_event_loop();
  prv_deinit();
}
