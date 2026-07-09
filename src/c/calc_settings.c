#include "calc_settings.h"

static bool s_haptic_feedback = true;
static bool s_keep_backlight = false;

bool calc_settings_haptic_enabled(void) {
  return s_haptic_feedback;
}

bool calc_settings_keep_backlight(void) {
  return s_keep_backlight;
}

void calc_settings_load(CalcEngine *engine) {
  if (persist_exists(PERSIST_KEY_RPN_MODE)) {
    engine->rpn_mode = persist_read_bool(PERSIST_KEY_RPN_MODE);
  }

  // Stack before main number: main number re-derives entry + X on top.
  if (persist_exists(PERSIST_KEY_STACK)) {
    persist_read_data(PERSIST_KEY_STACK, engine->stack, sizeof(engine->stack));
  }

  if (persist_exists(PERSIST_KEY_MAIN_NUMBER)) {
    double main_num = 0.0;
    persist_read_data(PERSIST_KEY_MAIN_NUMBER, &main_num, sizeof(double));
    calc_engine_set_main_number(engine, main_num);
  }

  if (persist_exists(PERSIST_KEY_HAPTIC_FEEDBACK)) {
    s_haptic_feedback = persist_read_bool(PERSIST_KEY_HAPTIC_FEEDBACK);
  }

  if (persist_exists(PERSIST_KEY_KEEP_BACKLIGHT)) {
    s_keep_backlight = persist_read_bool(PERSIST_KEY_KEEP_BACKLIGHT);
    light_enable(s_keep_backlight);
  }

  // DEG/RAD defaults to true via calc_engine_init.
  if (persist_exists(PERSIST_KEY_DEG_MODE)) {
    engine->deg_mode = persist_read_bool(PERSIST_KEY_DEG_MODE);
  }

  if (persist_exists(PERSIST_KEY_MEMORY)) {
    double mem = 0.0;
    persist_read_data(PERSIST_KEY_MEMORY, &mem, sizeof(double));
    engine->memory = mem;
  }
}

void calc_settings_save(CalcEngine *engine) {
  double main_num = calc_engine_get_main_number(engine);
  persist_write_data(PERSIST_KEY_MAIN_NUMBER, &main_num, sizeof(double));
  persist_write_data(PERSIST_KEY_MEMORY, &engine->memory, sizeof(double));
  persist_write_data(PERSIST_KEY_STACK, engine->stack, sizeof(engine->stack));
  // DEG/RAD can now change from the watch (DRG key), not just Clay config.
  persist_write_bool(PERSIST_KEY_DEG_MODE, engine->deg_mode);
}

void calc_settings_handle_inbox(DictionaryIterator *iter, CalcEngine *engine) {
  Tuple *rpn_tuple = dict_find(iter, MESSAGE_KEY_RPN_MODE);
  if (rpn_tuple) {
    bool rpn = rpn_tuple->value->int32 != 0;
    persist_write_bool(PERSIST_KEY_RPN_MODE, rpn);
    calc_engine_set_rpn_mode(engine, rpn);
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

  Tuple *deg_tuple = dict_find(iter, MESSAGE_KEY_DEG_MODE);
  if (deg_tuple) {
    bool deg = deg_tuple->value->int32 != 0;
    engine->deg_mode = deg;
    persist_write_bool(PERSIST_KEY_DEG_MODE, deg);
    APP_LOG(APP_LOG_LEVEL_INFO, "DEG mode set to %d", deg);
  }
}
