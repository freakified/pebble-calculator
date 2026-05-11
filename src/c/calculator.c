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
#define PERSIST_KEY_MAIN_NUMBER 3
#define PERSIST_KEY_KEEP_BACKLIGHT 4

// ---------------------------------------------------------------------------
// Static state
// ---------------------------------------------------------------------------

static Window *s_window;
static Layer *s_ui_layer;
static CalcEngine s_engine;
static int s_pressed_button = -1;
static bool s_haptic_feedback = true;
static bool s_keep_backlight = false;

static AppTimer *s_hold_timer = NULL;
static CalcAction s_held_button_action = CALC_ACTION_NONE;
static const int s_hold_duration = 300; // ms

static const uint32_t s_vibe_durations[] = {50};
static const VibePattern s_vibe_pattern = {
  .durations = s_vibe_durations,
  .num_segments = 1
};

// ---------------------------------------------------------------------------
// Long-press / long-touch handling
// ---------------------------------------------------------------------------

static void prv_hold_timer_callback(void *data) {
  s_hold_timer = NULL;
  calc_engine_handle_action(&s_engine, CALC_ACTION_CLEAR_ENTRY);
  if (s_haptic_feedback) {
    vibes_enqueue_custom_pattern(s_vibe_pattern);
  }
  s_pressed_button = -1;
  calc_ui_set_pressed(-1);
  calc_ui_mark_dirty();
}

static void prv_clear_hold_timer() {
  if (!s_hold_timer) return;
  app_timer_cancel(s_hold_timer);
  s_hold_timer = NULL;
  s_held_button_action = CALC_ACTION_NONE;
}

static void prv_set_hold_timer(CalcAction action) {
  prv_clear_hold_timer();
  s_hold_timer = app_timer_register(s_hold_duration, prv_hold_timer_callback, NULL);
  s_held_button_action = action;
}

// ---------------------------------------------------------------------------
// Touch handling
// ---------------------------------------------------------------------------

static void prv_touch_handler(const TouchEvent *event, void *context) {
  switch (event->type) {
    case TouchEvent_Touchdown: {
      CalcButton idx = calc_buttons_hit_test(GPoint(event->x, event->y));
      if (idx != CALC_BUTTON_NONE) {
        s_pressed_button = idx;
        calc_ui_set_pressed(idx);
        if (s_haptic_feedback) {
          vibes_enqueue_custom_pattern(s_vibe_pattern);
        }
        const CalcButtonInfo *btn = calc_buttons_get_info(s_pressed_button);
        if (btn && btn->long_press_action != CALC_ACTION_NONE) {
          prv_set_hold_timer(btn->long_press_action);
        }
        calc_ui_mark_dirty();
      }
      break;
    }

    case TouchEvent_PositionUpdate: {
      if (s_pressed_button >= 0) {
        const CalcButtonInfo *btn = calc_buttons_get_info(s_pressed_button);
        GPoint p = GPoint(event->x, event->y);
        if (btn && !grect_contains_point(&btn->bounds, &p)) {
          prv_clear_hold_timer();
          s_pressed_button = -1;
          calc_ui_set_pressed(-1);
          calc_ui_mark_dirty();
        }
      }
      break;
    }

    case TouchEvent_Liftoff: {
      prv_clear_hold_timer();
      if (s_pressed_button >= 0) {
        const CalcButtonInfo *btn = calc_buttons_get_info(s_pressed_button);
        if (btn) {
          calc_engine_handle_action(&s_engine, btn->action);
        }
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
  calc_engine_handle_action(&s_engine, CALC_ACTION_NEGATE);
  calc_ui_mark_dirty();
}

static void prv_up_click(ClickRecognizerRef recognizer, void *context) {
  if (s_engine.rpn_mode) {
    calc_engine_handle_action(&s_engine, CALC_ACTION_SWAP);
    calc_ui_mark_dirty();
  }
}

static void prv_down_click(ClickRecognizerRef recognizer, void *context) {
  calc_engine_handle_action(&s_engine, CALC_ACTION_BACKSPACE);
  calc_ui_mark_dirty();
}

static void prv_down_long_click(ClickRecognizerRef recognizer, void *context) {
  calc_engine_handle_action(&s_engine, CALC_ACTION_CLEAR_ENTRY);
  if (s_haptic_feedback) {
    vibes_enqueue_custom_pattern(s_vibe_pattern);
  }
  calc_ui_mark_dirty();
}

static void prv_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click);
  window_single_click_subscribe(BUTTON_ID_UP, prv_up_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_down_click);
  window_long_click_subscribe(BUTTON_ID_DOWN, s_hold_duration, prv_down_long_click, NULL);
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

  Tuple *backlight_tuple = dict_find(iter, MESSAGE_KEY_KEEP_BACKLIGHT);
  if (backlight_tuple) {
    s_keep_backlight = backlight_tuple->value->int32 != 0;
    persist_write_bool(PERSIST_KEY_KEEP_BACKLIGHT, s_keep_backlight);
    light_enable(s_keep_backlight);
    APP_LOG(APP_LOG_LEVEL_INFO, "Keep backlight set to %d", s_keep_backlight);
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

  // Restore main number
  if (persist_exists(PERSIST_KEY_MAIN_NUMBER)) {
    double main_num = 0.0;
    persist_read_data(PERSIST_KEY_MAIN_NUMBER, &main_num, sizeof(double));
    calc_engine_set_main_number(&s_engine, main_num);
  }

  // Restore haptic feedback setting
  if (persist_exists(PERSIST_KEY_HAPTIC_FEEDBACK)) {
    s_haptic_feedback = persist_read_bool(PERSIST_KEY_HAPTIC_FEEDBACK);
  }

  // Restore backlight setting
  if (persist_exists(PERSIST_KEY_KEEP_BACKLIGHT)) {
    s_keep_backlight = persist_read_bool(PERSIST_KEY_KEEP_BACKLIGHT);
    light_enable(s_keep_backlight);
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
  prv_clear_hold_timer();

  double main_num = calc_engine_get_main_number(&s_engine);
  persist_write_data(PERSIST_KEY_MAIN_NUMBER, &main_num, sizeof(double));
  light_enable(false);

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
