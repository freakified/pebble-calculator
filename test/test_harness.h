#pragma once

// Dependency-free test harness for the calculator engine.
//
// The engine's entire input surface is calc_engine_handle_action(engine,
// action); its output is read back through the display getters. So a test is
// just: reset the engine, feed a sequence of actions, assert on the display.
//
// Two ways to feed input:
//   feed(e, "2+3*4=")       - ergonomic string for infix digit/operator entry
//   FEED(e, DIGIT_3, ENTER) - explicit CalcAction list for RPN / scientific ops
//
// Assertions compare the formatted display strings the device would show, so a
// green suite here means the same numbers render on-watch (host double is
// IEEE-754 identical, and calc_math/calc_format are the same pure-C sources).

#include <stdio.h>
#include <string.h>
#include "calc_engine.h"

// ---------------------------------------------------------------------------
// Result tracking
// ---------------------------------------------------------------------------

static int g_pass = 0;
static int g_fail = 0;
static const char *g_group = "";

// Name the group of cases that follow; printed once, and prefixed to failures.
static inline void group(const char *name) { g_group = name; }

static inline void check_eq(const char *file, int line,
                            const char *got, const char *want) {
  if (strcmp(got, want) == 0) {
    g_pass++;
  } else {
    g_fail++;
    printf("  FAIL %s:%d  [%s]  got \"%s\"  want \"%s\"\n",
           file, line, g_group, got, want);
  }
}

static inline void check_prefix(const char *file, int line,
                                const char *got, const char *prefix) {
  if (strncmp(got, prefix, strlen(prefix)) == 0) {
    g_pass++;
  } else {
    g_fail++;
    printf("  FAIL %s:%d  [%s]  got \"%s\"  want prefix \"%s\"\n",
           file, line, g_group, got, prefix);
  }
}

// Exit status 0 iff everything passed — CI/git-hook friendly.
static inline int report(void) {
  printf("\n%d passed, %d failed\n", g_pass, g_fail);
  return g_fail ? 1 : 0;
}

// ---------------------------------------------------------------------------
// Assertions
// ---------------------------------------------------------------------------

// X register / current entry (the big number on screen).
#define ASSERT_DISPLAY(e, want) \
  check_eq(__FILE__, __LINE__, calc_engine_get_x_display(e), (want))

// X register begins with prefix — for transcendentals whose full rounding we
// don't want to pin down (e.g. sqrt(2), pi).
#define ASSERT_PREFIX(e, prefix) \
  check_prefix(__FILE__, __LINE__, calc_engine_get_x_display(e), (prefix))

// RPN stack register: 0=T, 1=Z, 2=Y (X is the entry display).
#define ASSERT_STACK(e, reg, want)                          \
  do {                                                      \
    char _b[CALC_FORMAT_BUF_SIZE];                          \
    calc_engine_get_stack_display((e), (reg), _b, sizeof _b); \
    check_eq(__FILE__, __LINE__, _b, (want));               \
  } while (0)

// Standard-mode secondary line (pending op-stack, or the post-'=' tape).
#define ASSERT_SECONDARY(e, want)                           \
  do {                                                      \
    char _b[CALC_SECONDARY_BUF_SIZE];                       \
    calc_engine_get_secondary_display((e), _b, sizeof _b);  \
    check_eq(__FILE__, __LINE__, _b, (want));               \
  } while (0)

// ---------------------------------------------------------------------------
// Engine setup
// ---------------------------------------------------------------------------

static inline void reset_std(CalcEngine *e) {
  calc_engine_init(e);
  calc_engine_set_rpn_mode(e, false);
}

static inline void reset_rpn(CalcEngine *e) {
  calc_engine_init(e);
  calc_engine_set_rpn_mode(e, true);
}

// ---------------------------------------------------------------------------
// Input helpers
// ---------------------------------------------------------------------------

// Feed a string of keypresses. Character map (spaces ignored for readability):
//   0-9 . -> digits / decimal point       + - -> add / subtract
//   * or x -> multiply    / -> divide      ^ -> power (y^x)
//   = -> equals/evaluate  c -> clear (C)   < -> backspace   ~ -> negate (+/-)
static inline void feed(CalcEngine *e, const char *keys) {
  for (const char *p = keys; *p; p++) {
    CalcAction a;
    char k = *p;
    if (k >= '0' && k <= '9') {
      a = (CalcAction)(CALC_ACTION_DIGIT_0 + (k - '0'));
    } else {
      switch (k) {
        case '.': a = CALC_ACTION_DOT;       break;
        case '+': a = CALC_ACTION_ADD;       break;
        case '-': a = CALC_ACTION_SUBTRACT;  break;
        case '*':
        case 'x': a = CALC_ACTION_MULTIPLY;  break;
        case '/': a = CALC_ACTION_DIVIDE;    break;
        case '^': a = CALC_ACTION_POW;       break;
        case '%': a = CALC_ACTION_PERCENT;   break;
        case '=': a = CALC_ACTION_EQUALS;    break;
        case 'c': a = CALC_ACTION_CLEAR;     break;
        case '<': a = CALC_ACTION_BACKSPACE; break;
        case '~': a = CALC_ACTION_NEGATE;    break;
        case ' ': continue;
        default:
          printf("  FAIL harness: feed() saw unmapped char '%c'\n", k);
          g_fail++;
          continue;
      }
    }
    calc_engine_handle_action(e, a);
  }
}

// Feed an explicit list of CalcAction values (for RPN / scientific ops that
// have no convenient character). Usage: FEED(e, DIGIT_3, ENTER, DIGIT_4, ADD);
// Names are the CalcAction enum with the CALC_ACTION_ prefix dropped.
static inline void feed_actions(CalcEngine *e, const CalcAction *acts, int n) {
  for (int i = 0; i < n; i++) calc_engine_handle_action(e, acts[i]);
}

#define FEED(e, ...)                                                     \
  do {                                                                   \
    const CalcAction _acts[] = { __VA_ARGS__ };                          \
    feed_actions((e), _acts, (int)(sizeof _acts / sizeof _acts[0]));     \
  } while (0)

// Short aliases so FEED(...) lists read cleanly.
#define DIGIT_0 CALC_ACTION_DIGIT_0
#define DIGIT_1 CALC_ACTION_DIGIT_1
#define DIGIT_2 CALC_ACTION_DIGIT_2
#define DIGIT_3 CALC_ACTION_DIGIT_3
#define DIGIT_4 CALC_ACTION_DIGIT_4
#define DIGIT_5 CALC_ACTION_DIGIT_5
#define DIGIT_6 CALC_ACTION_DIGIT_6
#define DIGIT_7 CALC_ACTION_DIGIT_7
#define DIGIT_8 CALC_ACTION_DIGIT_8
#define DIGIT_9 CALC_ACTION_DIGIT_9
#define DOT     CALC_ACTION_DOT
#define ADD     CALC_ACTION_ADD
#define SUB     CALC_ACTION_SUBTRACT
#define MUL     CALC_ACTION_MULTIPLY
#define DIV     CALC_ACTION_DIVIDE
#define NEGATE  CALC_ACTION_NEGATE
#define EQUALS  CALC_ACTION_EQUALS
#define ENTER   CALC_ACTION_ENTER
#define SWAP    CALC_ACTION_SWAP
#define DROP    CALC_ACTION_DROP
#define ROLL_DOWN CALC_ACTION_ROLL_DOWN
#define ROLL_UP   CALC_ACTION_ROLL_UP
#define LAST_X    CALC_ACTION_LAST_X
#define SQRT    CALC_ACTION_SQRT
#define SQUARE  CALC_ACTION_SQUARE
#define RECIP   CALC_ACTION_RECIP
#define FACT    CALC_ACTION_FACT
#define SIN     CALC_ACTION_SIN
#define COS     CALC_ACTION_COS
#define TAN     CALC_ACTION_TAN
#define ASIN    CALC_ACTION_ASIN
#define ACOS    CALC_ACTION_ACOS
#define ATAN    CALC_ACTION_ATAN
#define LN      CALC_ACTION_LN
#define LOG10   CALC_ACTION_LOG10
#define EXP     CALC_ACTION_EXP
#define POW10   CALC_ACTION_POW10
#define POW     CALC_ACTION_POW
#define NTHROOT CALC_ACTION_NTHROOT
#define TO_HMS  CALC_ACTION_TO_HMS
#define TO_H    CALC_ACTION_TO_H
#define EE      CALC_ACTION_EE
#define PI      CALC_ACTION_PI
#define EULER_E CALC_ACTION_E
#define PERCENT CALC_ACTION_PERCENT
#define DRG_TOGGLE CALC_ACTION_DRG_TOGGLE
#define SECOND  CALC_ACTION_2ND_TOGGLE
#define M_PLUS   CALC_ACTION_M_PLUS
#define M_MINUS  CALC_ACTION_M_MINUS
#define M_RECALL CALC_ACTION_M_RECALL
#define M_CLEAR  CALC_ACTION_M_CLEAR
