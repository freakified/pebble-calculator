#include "calc_settings.h"

static bool s_haptic_feedback = true;
static bool s_keep_backlight = false;
static bool s_swipe_paging = true;
static int s_key_up = KEYFUNC_BACKSPACE;
static int s_key_select = KEYFUNC_PAGE_NEXT;
static int s_key_down = KEYFUNC_NEGATE;

bool calc_settings_haptic_enabled(void) {
  return s_haptic_feedback;
}

bool calc_settings_keep_backlight(void) {
  return s_keep_backlight;
}

bool calc_settings_swipe_paging_enabled(void) {
  return s_swipe_paging;
}

// Map a curated KeyFunc id (persisted / from Clay) to the CalcAction the engine
// executes. Unknown ids and KEYFUNC_NONE resolve to a no-op.
static CalcAction prv_resolve_keyfunc(int kf) {
  switch (kf) {
  case KEYFUNC_PAGE_NEXT:  return CALC_ACTION_PAGE_NEXT;
  case KEYFUNC_PAGE_PREV:  return CALC_ACTION_PAGE_PREV;
  case KEYFUNC_BACKSPACE:  return CALC_ACTION_BACKSPACE;
  case KEYFUNC_CLEAR:      return CALC_ACTION_CLEAR;
  case KEYFUNC_NEGATE:     return CALC_ACTION_NEGATE;
  case KEYFUNC_EQUALS:     return CALC_ACTION_EQUALS;
  case KEYFUNC_ENTER:      return CALC_ACTION_ENTER;
  case KEYFUNC_SWAP:       return CALC_ACTION_SWAP;
  case KEYFUNC_ROLL_DOWN:  return CALC_ACTION_ROLL_DOWN;
  case KEYFUNC_ROLL_UP:    return CALC_ACTION_ROLL_UP;
  case KEYFUNC_DROP:       return CALC_ACTION_DROP;
  case KEYFUNC_LAST_X:     return CALC_ACTION_LAST_X;
  case KEYFUNC_M_RECALL:   return CALC_ACTION_M_RECALL;
  case KEYFUNC_M_PLUS:     return CALC_ACTION_M_PLUS;
  case KEYFUNC_M_MINUS:    return CALC_ACTION_M_MINUS;
  case KEYFUNC_M_CLEAR:    return CALC_ACTION_M_CLEAR;
  case KEYFUNC_DRG_TOGGLE: return CALC_ACTION_DRG_TOGGLE;
  case KEYFUNC_2ND_TOGGLE: return CALC_ACTION_2ND_TOGGLE;
  case KEYFUNC_PI:         return CALC_ACTION_PI;
  case KEYFUNC_E:          return CALC_ACTION_E;
  case KEYFUNC_ADD:        return CALC_ACTION_ADD;
  case KEYFUNC_SUBTRACT:   return CALC_ACTION_SUBTRACT;
  case KEYFUNC_MULTIPLY:   return CALC_ACTION_MULTIPLY;
  case KEYFUNC_DIVIDE:     return CALC_ACTION_DIVIDE;
  case KEYFUNC_SQRT:       return CALC_ACTION_SQRT;
  case KEYFUNC_SQUARE:     return CALC_ACTION_SQUARE;
  case KEYFUNC_RECIP:      return CALC_ACTION_RECIP;
  case KEYFUNC_STACK_CLEAR: return CALC_ACTION_STACK_CLEAR;
  case KEYFUNC_PERCENT:    return CALC_ACTION_PERCENT;
  case KEYFUNC_EE:         return CALC_ACTION_EE;
  case KEYFUNC_NONE:
  default:                 return CALC_ACTION_NOOP;
  }
}

// Clay `select` values arrive as C strings (HTML <option> values are always
// strings), not integers — so read the KeyFunc id defensively, accepting either
// representation.
static int prv_read_keyfunc_tuple(const Tuple *t) {
  if (t->type == TUPLE_CSTRING) {
    return atoi(t->value->cstring);
  }
  return t->value->int32;
}

CalcAction calc_settings_key_up_action(void) {
  return prv_resolve_keyfunc(s_key_up);
}

CalcAction calc_settings_key_select_action(void) {
  return prv_resolve_keyfunc(s_key_select);
}

CalcAction calc_settings_key_down_action(void) {
  return prv_resolve_keyfunc(s_key_down);
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

  if (persist_exists(PERSIST_KEY_SWIPE_PAGING)) {
    s_swipe_paging = persist_read_bool(PERSIST_KEY_SWIPE_PAGING);
  }

  // Only accept a stored id in the valid KeyFunc range; anything else keeps the
  // default. This also self-heals data written by an earlier build that stored
  // Clay's string select values as garbage ints.
  if (persist_exists(PERSIST_KEY_ACTION_UP)) {
    int v = persist_read_int(PERSIST_KEY_ACTION_UP);
    if (v >= KEYFUNC_NONE && v <= KEYFUNC_MAX_) {
      s_key_up = v;
    }
  }
  if (persist_exists(PERSIST_KEY_ACTION_SELECT)) {
    int v = persist_read_int(PERSIST_KEY_ACTION_SELECT);
    if (v >= KEYFUNC_NONE && v <= KEYFUNC_MAX_) {
      s_key_select = v;
    }
  }
  if (persist_exists(PERSIST_KEY_ACTION_DOWN)) {
    int v = persist_read_int(PERSIST_KEY_ACTION_DOWN);
    if (v >= KEYFUNC_NONE && v <= KEYFUNC_MAX_) {
      s_key_down = v;
    }
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

  Tuple *swipe_tuple = dict_find(iter, MESSAGE_KEY_SWIPE_PAGING);
  if (swipe_tuple) {
    s_swipe_paging = swipe_tuple->value->int32 != 0;
    persist_write_bool(PERSIST_KEY_SWIPE_PAGING, s_swipe_paging);
    APP_LOG(APP_LOG_LEVEL_INFO, "Swipe paging set to %d", s_swipe_paging);
  }

  Tuple *key_up_tuple = dict_find(iter, MESSAGE_KEY_KEY_UP_ACTION);
  if (key_up_tuple) {
    s_key_up = prv_read_keyfunc_tuple(key_up_tuple);
    persist_write_int(PERSIST_KEY_ACTION_UP, s_key_up);
    APP_LOG(APP_LOG_LEVEL_INFO, "UP key action set to %d", s_key_up);
  }

  Tuple *key_select_tuple = dict_find(iter, MESSAGE_KEY_KEY_SELECT_ACTION);
  if (key_select_tuple) {
    s_key_select = prv_read_keyfunc_tuple(key_select_tuple);
    persist_write_int(PERSIST_KEY_ACTION_SELECT, s_key_select);
    APP_LOG(APP_LOG_LEVEL_INFO, "SELECT key action set to %d", s_key_select);
  }

  Tuple *key_down_tuple = dict_find(iter, MESSAGE_KEY_KEY_DOWN_ACTION);
  if (key_down_tuple) {
    s_key_down = prv_read_keyfunc_tuple(key_down_tuple);
    persist_write_int(PERSIST_KEY_ACTION_DOWN, s_key_down);
    APP_LOG(APP_LOG_LEVEL_INFO, "DOWN key action set to %d", s_key_down);
  }
}
