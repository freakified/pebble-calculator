// Off-device test suite for the calculator engine.
//
// Build & run:  make test   (from the repo root)
//
// Drives calc_engine_handle_action() directly and asserts on the display
// getters — no emulator or button injection required. See test_harness.h for
// the feed()/FEED()/ASSERT_* helpers.
//
// Expected values were verified against known mathematics at authoring time.
// Because the on-device math (calc_math.c) is a pure-C Taylor/Newton library,
// a handful of results carry tiny approximation residues (e.g. cos(90°) is not
// exactly 0); those are asserted as-is and called out in test_precision_notes()
// so a future change there is recognized as deliberate, not a regression.

#include "test_harness.h"
#include "calc_format.h"

// ===========================================================================
// Standard (infix) mode
// ===========================================================================

static void test_infix_basic(CalcEngine *e) {
  group("infix basic");

  reset_std(e); feed(e, "2+3=");        ASSERT_DISPLAY(e, "5");
  reset_std(e); feed(e, "9-4=");        ASSERT_DISPLAY(e, "5");
  reset_std(e); feed(e, "6*7=");        ASSERT_DISPLAY(e, "42");
  reset_std(e); feed(e, "20/4=");       ASSERT_DISPLAY(e, "5");
  reset_std(e); feed(e, "1.5+2.5=");    ASSERT_DISPLAY(e, "4");
  reset_std(e); feed(e, "10/4=");       ASSERT_DISPLAY(e, "2.5");
  reset_std(e); feed(e, "1/3=");        ASSERT_DISPLAY(e, "0.33333333333");

  // Chained same-precedence operators evaluate left to right.
  reset_std(e); feed(e, "1+2+3+4=");    ASSERT_DISPLAY(e, "10");
  reset_std(e); feed(e, "100-1-2-3=");  ASSERT_DISPLAY(e, "94");
  reset_std(e); feed(e, "100/4/5=");    ASSERT_DISPLAY(e, "5");
}

static void test_infix_precedence(CalcEngine *e) {
  group("infix precedence");

  // * binds tighter than +.
  reset_std(e); feed(e, "2+3*4=");      ASSERT_DISPLAY(e, "14");
  reset_std(e); feed(e, "2*3+4=");      ASSERT_DISPLAY(e, "10");
  // Three levels: 1 + (2 * (3^2)) = 1 + 18 = 19.
  reset_std(e); feed(e, "1+2*3^2=");    ASSERT_DISPLAY(e, "19");
  // ^ is right-associative: 2^(3^2) = 2^9 = 512, and 2^(2^3) = 2^8 = 256.
  reset_std(e); feed(e, "2^3^2=");      ASSERT_DISPLAY(e, "512");
  reset_std(e); feed(e, "2^2^3=");      ASSERT_DISPLAY(e, "256");
}

static void test_infix_chains(CalcEngine *e) {
  group("infix complex chains");

  reset_std(e); feed(e, "2+3*4-6/2=");  ASSERT_DISPLAY(e, "11"); // 2+12-3
  reset_std(e); feed(e, "2*3^2+1=");    ASSERT_DISPLAY(e, "19"); // 18+1
  reset_std(e); feed(e, "10-2*3+4/2=");  ASSERT_DISPLAY(e, "6");  // 10-6+2
  reset_std(e); feed(e, "1+2*3^2-4=");   ASSERT_DISPLAY(e, "15"); // 1+18-4

  // Result of '=' carries into a new operator (chained equals).
  reset_std(e); feed(e, "2+3="); feed(e, "*4=");  ASSERT_DISPLAY(e, "20");
  // A unary applied to the just-computed result.
  reset_std(e); feed(e, "3*4="); FEED(e, SQRT);   ASSERT_DISPLAY(e, "3.46410161514"); // sqrt(12)
}

static void test_infix_entry_editing(CalcEngine *e) {
  group("infix entry editing");

  reset_std(e); feed(e, "5~");          ASSERT_DISPLAY(e, "-5");
  reset_std(e); feed(e, "5~~");         ASSERT_DISPLAY(e, "5");
  reset_std(e); feed(e, "123<");        ASSERT_DISPLAY(e, "12");
  reset_std(e); feed(e, "12<<");        ASSERT_DISPLAY(e, "0");
  reset_std(e); feed(e, "007");         ASSERT_DISPLAY(e, "7");
  reset_std(e); feed(e, ".5");          ASSERT_DISPLAY(e, "0.5");
  // Changing one's mind about the operator: 5 (+ then ×) 3 = 15.
  reset_std(e); feed(e, "5+*3=");       ASSERT_DISPLAY(e, "15");
}

static void test_infix_errors(CalcEngine *e) {
  group("infix errors");

  reset_std(e); feed(e, "5/0=");        ASSERT_DISPLAY(e, "Error");
  reset_std(e); feed(e, "5/0=8+1=");    ASSERT_DISPLAY(e, "9"); // digit after error starts clean
}

static void test_infix_secondary(CalcEngine *e) {
  group("infix secondary line");

  reset_std(e); feed(e, "2+");          ASSERT_SECONDARY(e, "2 +");
  reset_std(e); feed(e, "2+3*");        ASSERT_SECONDARY(e, "2 + 3 x");
  reset_std(e); feed(e, "2+3=");        ASSERT_SECONDARY(e, "2 + 3 =");
}

static void test_infix_clear(CalcEngine *e) {
  group("infix clear / memory");

  // C clears the entry but preserves memory; AC (second C, once the entry is
  // committed) wipes it. "5+0=" commits 5 into a non-entering state.
  reset_std(e); feed(e, "5+0="); FEED(e, M_PLUS);
  feed(e, "c"); FEED(e, M_RECALL);      ASSERT_DISPLAY(e, "5");
  reset_std(e); feed(e, "5+0="); FEED(e, M_PLUS);
  feed(e, "cc"); FEED(e, M_RECALL);     ASSERT_DISPLAY(e, "0");

  // Backspace and Clear are interchangeable once the entry is committed: an
  // idle Backspace escalates C -> AC just like a second Clear does.
  reset_std(e); feed(e, "5+0="); FEED(e, M_PLUS);
  feed(e, "<<"); FEED(e, M_RECALL);     ASSERT_DISPLAY(e, "0"); // < then < = C then AC

  // just_cleared is sticky through AC: once everything's gone the button stays
  // on "AC" rather than flip-flopping back to "C" on a third press.
  reset_std(e); feed(e, "5+0=");
  ASSERT_JUST_CLEARED(e, false);
  feed(e, "c");   ASSERT_JUST_CLEARED(e, true);  // C
  feed(e, "c");   ASSERT_JUST_CLEARED(e, true);  // AC (was false before the fix)
  feed(e, "c");   ASSERT_JUST_CLEARED(e, true);  // stays AC
  // Any non-clear action releases the flag, so C is offered again next time.
  FEED(e, DIGIT_7); ASSERT_JUST_CLEARED(e, false);
}

// ===========================================================================
// RPN clear (CLx -> AC)
// ===========================================================================

static void test_rpn_clear(CalcEngine *e) {
  group("rpn clear (CLx / AC)");

  // While mid-entry, Backspace just trims digits; it only reaches the CLx/AC
  // path once the entry is committed (here via ENTER, which leaves X = Y = 3).
  reset_rpn(e); FEED(e, DIGIT_2, ENTER, DIGIT_3, ENTER);
  ASSERT_DISPLAY(e, "3"); ASSERT_STACK(e, 2, "3");
  FEED(e, BACKSPACE);                                  // CLx: X only, Y survives
  ASSERT_DISPLAY(e, "0"); ASSERT_STACK(e, 2, "3"); ASSERT_JUST_CLEARED(e, true);
  FEED(e, BACKSPACE);                                  // AC: whole stack
  ASSERT_DISPLAY(e, "0"); ASSERT_STACK(e, 2, "0"); ASSERT_JUST_CLEARED(e, true);
  FEED(e, BACKSPACE);                                  // stays AC
  ASSERT_JUST_CLEARED(e, true);

  // AC also wipes the memory register (CLx alone leaves it alone).
  reset_rpn(e); FEED(e, DIGIT_9, M_PLUS, ENTER);       // memory = 9
  FEED(e, BACKSPACE, BACKSPACE, M_RECALL);             // CLx then AC
  ASSERT_DISPLAY(e, "0");
}

// ===========================================================================
// Percent
// ===========================================================================

static void test_percent(CalcEngine *e) {
  group("percent");

  // Desk-calculator convention: with pending +/- it's a percentage OF the left
  // operand; with ×/÷ (or none) it's a bare x/100 factor.
  reset_std(e); feed(e, "200+10%=");    ASSERT_DISPLAY(e, "220"); // +20
  reset_std(e); feed(e, "200-10%=");    ASSERT_DISPLAY(e, "180"); // -20
  reset_std(e); feed(e, "200*10%=");    ASSERT_DISPLAY(e, "20");  // *0.1
  reset_std(e); feed(e, "200/10%=");    ASSERT_DISPLAY(e, "2000");// /0.1
  reset_std(e); feed(e, "50%");         ASSERT_DISPLAY(e, "0.5"); // bare
  reset_std(e); feed(e, "80+25%=");     ASSERT_DISPLAY(e, "100");
  reset_std(e); feed(e, "100+50%+10%="); ASSERT_DISPLAY(e, "165"); // 150 then +10% of 150

  // RPN: X <- Y*X/100, with Y preserved.
  reset_rpn(e); FEED(e, DIGIT_2, DIGIT_0, DIGIT_0, ENTER, DIGIT_1, DIGIT_0, PERCENT);
  ASSERT_DISPLAY(e, "20");
  ASSERT_STACK(e, 2, "200");
}

// ===========================================================================
// Scientific functions
// ===========================================================================

static void test_trig(CalcEngine *e) {
  group("trig (degrees default)");

  reset_std(e); FEED(e, DIGIT_3, DIGIT_0, SIN);   ASSERT_DISPLAY(e, "0.5");
  reset_std(e); FEED(e, DIGIT_9, DIGIT_0, SIN);   ASSERT_DISPLAY(e, "1");
  reset_std(e); FEED(e, DIGIT_4, DIGIT_5, SIN);   ASSERT_DISPLAY(e, "0.70710678119");
  reset_std(e); FEED(e, DIGIT_0, COS);            ASSERT_DISPLAY(e, "1");
  reset_std(e); FEED(e, DIGIT_6, DIGIT_0, COS);   ASSERT_DISPLAY(e, "0.5");
  reset_std(e); FEED(e, DIGIT_4, DIGIT_5, TAN);   ASSERT_DISPLAY(e, "1");
  reset_std(e); FEED(e, DIGIT_0, TAN);            ASSERT_DISPLAY(e, "0");
  reset_std(e); FEED(e, DIGIT_6, DIGIT_0, TAN);   ASSERT_DISPLAY(e, "1.73205080757"); // sqrt(3)

  // Near-zero results at the 90/180/270 crossings snap to exactly 0 rather than
  // leaking argument-reduction noise (e.g. cos(90) was ~4.6e-17). See
  // prv_snap_zero() in calc_engine_sci.c.
  reset_std(e); FEED(e, DIGIT_9, DIGIT_0, COS);            ASSERT_DISPLAY(e, "0");
  reset_std(e); FEED(e, DIGIT_1, DIGIT_8, DIGIT_0, SIN);   ASSERT_DISPLAY(e, "0");
  reset_std(e); FEED(e, DIGIT_2, DIGIT_7, DIGIT_0, COS);   ASSERT_DISPLAY(e, "0");
  reset_std(e); FEED(e, DIGIT_1, DIGIT_8, DIGIT_0, TAN);   ASSERT_DISPLAY(e, "0");
  reset_std(e); FEED(e, DIGIT_1, DIGIT_8, DIGIT_0, COS);   ASSERT_DISPLAY(e, "-1"); // not snapped

  // tan is undefined at its 90+180k asymptotes: error, not a spurious ~2e16.
  reset_std(e); FEED(e, DIGIT_9, DIGIT_0, TAN);            ASSERT_DISPLAY(e, "Error");
  reset_std(e); FEED(e, DIGIT_2, DIGIT_7, DIGIT_0, TAN);   ASSERT_DISPLAY(e, "Error");
  reset_std(e); FEED(e, DIGIT_9, DIGIT_0, NEGATE, TAN);    ASSERT_DISPLAY(e, "Error"); // -90
  reset_std(e); FEED(e, DIGIT_4, DIGIT_5, DIGIT_0, TAN);   ASSERT_DISPLAY(e, "Error"); // 450 = 90+360
  // ...but large finite tangents near (not at) the asymptote stay valid.
  reset_std(e); FEED(e, DIGIT_8, DIGIT_9, TAN);            ASSERT_DISPLAY(e, "57.2899616308");

  group("trig (radians)");
  reset_std(e); FEED(e, DRG_TOGGLE, DIGIT_1, SIN); ASSERT_DISPLAY(e, "0.84147098481");
  reset_std(e); FEED(e, DRG_TOGGLE, DIGIT_1, COS); ASSERT_DISPLAY(e, "0.54030230587");
  // DEG and RAD genuinely differ for the same argument.
  reset_std(e); FEED(e, DIGIT_3, DIGIT_0, SIN);            ASSERT_DISPLAY(e, "0.5");
  reset_std(e); FEED(e, DRG_TOGGLE, DIGIT_3, DIGIT_0, SIN); ASSERT_PREFIX(e, "-0.98");
}

static void test_inverse_trig(CalcEngine *e) {
  group("inverse trig (degrees)");

  reset_std(e); feed(e, ".5"); FEED(e, ASIN);     ASSERT_DISPLAY(e, "30");
  reset_std(e); FEED(e, DIGIT_1, ASIN);           ASSERT_DISPLAY(e, "90");
  reset_std(e); FEED(e, DIGIT_0, ACOS);           ASSERT_DISPLAY(e, "90");
  reset_std(e); FEED(e, DIGIT_1, ACOS);           ASSERT_DISPLAY(e, "0");
  reset_std(e); FEED(e, DIGIT_1, ATAN);           ASSERT_DISPLAY(e, "45");
  // Out-of-domain asin/acos raise an error.
  reset_std(e); FEED(e, DIGIT_2, ASIN);           ASSERT_DISPLAY(e, "Error");
}

static void test_log_exp(CalcEngine *e) {
  group("log / exp");

  reset_std(e); FEED(e, DIGIT_1, LN);             ASSERT_DISPLAY(e, "0");
  reset_std(e); FEED(e, EULER_E, LN);             ASSERT_DISPLAY(e, "1");      // ln(e)
  reset_std(e); FEED(e, DIGIT_1, DIGIT_0, LN);    ASSERT_DISPLAY(e, "2.30258509299");
  reset_std(e); FEED(e, DIGIT_0, LN);             ASSERT_DISPLAY(e, "Error");  // ln(0)
  reset_std(e); FEED(e, DIGIT_1, LOG10);          ASSERT_DISPLAY(e, "0");
  reset_std(e); FEED(e, DIGIT_1, DIGIT_0, DIGIT_0, LOG10);            ASSERT_DISPLAY(e, "2");
  reset_std(e); FEED(e, DIGIT_1, DIGIT_0, DIGIT_0, DIGIT_0, LOG10);   ASSERT_DISPLAY(e, "3");
  reset_std(e); FEED(e, DIGIT_0, EXP);            ASSERT_DISPLAY(e, "1");
  reset_std(e); FEED(e, DIGIT_1, EXP);            ASSERT_DISPLAY(e, "2.71828182846"); // e
  reset_std(e); FEED(e, DIGIT_2, EXP);            ASSERT_DISPLAY(e, "7.38905609893");
  reset_std(e); FEED(e, DIGIT_2, POW10);          ASSERT_DISPLAY(e, "100");
  reset_std(e); FEED(e, DIGIT_3, POW10);          ASSERT_DISPLAY(e, "1000");
}

static void test_powers_roots(CalcEngine *e) {
  group("powers / roots");

  reset_std(e); FEED(e, DIGIT_9, SQRT);           ASSERT_DISPLAY(e, "3");
  reset_std(e); FEED(e, DIGIT_2, SQRT);           ASSERT_DISPLAY(e, "1.41421356237");
  reset_std(e); FEED(e, DIGIT_7, SQUARE);         ASSERT_DISPLAY(e, "49");
  reset_std(e); feed(e, "1.5"); FEED(e, SQUARE);  ASSERT_DISPLAY(e, "2.25");
  reset_std(e); FEED(e, DIGIT_8, RECIP);          ASSERT_DISPLAY(e, "0.125");
  reset_std(e); FEED(e, DIGIT_3, RECIP);          ASSERT_DISPLAY(e, "0.33333333333");
  reset_std(e); FEED(e, DIGIT_0, RECIP);          ASSERT_DISPLAY(e, "Error"); // 1/0

  // Binary y^x and x-th root.
  reset_std(e); feed(e, "2^10=");                 ASSERT_DISPLAY(e, "1024");
  reset_std(e); feed(e, "2^.5=");                 ASSERT_DISPLAY(e, "1.41421356237"); // sqrt(2)
  reset_std(e); FEED(e, DIGIT_9, NTHROOT, DIGIT_2, EQUALS);            ASSERT_DISPLAY(e, "3"); // sqrt of 9
  reset_std(e); FEED(e, DIGIT_2, DIGIT_7, NTHROOT, DIGIT_3, EQUALS);   ASSERT_DISPLAY(e, "3"); // cube root of 27
}

static void test_factorial(CalcEngine *e) {
  group("factorial");

  reset_std(e); FEED(e, DIGIT_5, FACT);           ASSERT_DISPLAY(e, "120");
  reset_std(e); FEED(e, DIGIT_0, FACT);           ASSERT_DISPLAY(e, "1");
  reset_std(e); FEED(e, DIGIT_1, DIGIT_0, FACT);  ASSERT_DISPLAY(e, "3628800");
  reset_std(e); FEED(e, DIGIT_2, DIGIT_5, FACT);  ASSERT_DISPLAY(e, "1.55112e25");
  reset_std(e); FEED(e, DIGIT_1, DIGIT_7, DIGIT_0, FACT); ASSERT_DISPLAY(e, "7.2574e306"); // largest
  reset_std(e); FEED(e, DIGIT_1, DIGIT_7, DIGIT_1, FACT); ASSERT_DISPLAY(e, "Error");      // overflow
  reset_std(e); feed(e, "3.5"); FEED(e, FACT);    ASSERT_DISPLAY(e, "Error");              // non-integer
}

static void test_hms(CalcEngine *e) {
  group("HMS conversions");

  // 1.755 decimal hours = 1h 45m 18s -> 1.4518, and back.
  reset_std(e); feed(e, "1.755"); FEED(e, TO_HMS);  ASSERT_DISPLAY(e, "1.4518");
  reset_std(e); feed(e, "1.4518"); FEED(e, TO_H);   ASSERT_DISPLAY(e, "1.755");
  // 2.5 hours = 2h 30m -> 2.30.
  reset_std(e); feed(e, "2.5"); FEED(e, TO_HMS);    ASSERT_DISPLAY(e, "2.3");
}

static void test_constants(CalcEngine *e) {
  group("constants");

  reset_std(e); FEED(e, PI);                      ASSERT_DISPLAY(e, "3.14159265359");
  reset_std(e); FEED(e, EULER_E);                 ASSERT_DISPLAY(e, "2.71828182846");
  // Constant usable as an operand: 2*pi.
  reset_std(e); FEED(e, PI); feed(e, "*2=");      ASSERT_DISPLAY(e, "6.28318530718");
}

static void test_second_modifier(CalcEngine *e) {
  group("2nd-modifier inverses");

  reset_std(e); feed(e, ".5"); FEED(e, SECOND, SIN);   ASSERT_DISPLAY(e, "30");  // asin(0.5)
  reset_std(e); FEED(e, DIGIT_9, SECOND, SQRT);        ASSERT_DISPLAY(e, "81");  // square(9)
  reset_std(e); FEED(e, DIGIT_5, SECOND, RECIP);       ASSERT_DISPLAY(e, "120"); // 5!
}

static void test_exponent_entry(CalcEngine *e) {
  group("EE / scientific notation");

  // Raw entry buffer while typing an EE literal.
  reset_std(e); FEED(e, DIGIT_1, EE, DIGIT_5);            ASSERT_DISPLAY(e, "1e5");
  reset_std(e); FEED(e, DIGIT_6, DOT, DIGIT_0, DIGIT_2, EE, DIGIT_2, DIGIT_3);
  ASSERT_DISPLAY(e, "6.02e23");
  reset_std(e); FEED(e, DIGIT_1, EE, DIGIT_2, DIGIT_0, NEGATE); ASSERT_DISPLAY(e, "1e-20");

  // The literal's value participates in arithmetic and re-formats correctly.
  reset_std(e); FEED(e, DIGIT_1, EE, DIGIT_5); feed(e, "+0=");  ASSERT_DISPLAY(e, "100000");
  reset_std(e); FEED(e, DIGIT_6, DOT, DIGIT_0, DIGIT_2, EE, DIGIT_2, DIGIT_3);
  feed(e, "*1=");                                              ASSERT_DISPLAY(e, "6.02e23");
  reset_std(e); FEED(e, DIGIT_1, EE, DIGIT_2, DIGIT_0); feed(e, "="); FEED(e, SQUARE);
  ASSERT_DISPLAY(e, "1e40");

  // Plain results overflow into scientific notation at the display boundary,
  // with correct rounding (9.9999...e12 rounds up to 1e13).
  reset_std(e); feed(e, "123456789012*10=");  ASSERT_DISPLAY(e, "1.23457e12");
  reset_std(e); feed(e, "999999999999*10=");  ASSERT_DISPLAY(e, "1e13");
}

// Documents remaining floating-point residues from the pure-C math library.
// These are NOT bugs in the engine's dispatch/state logic — they are the limit
// of the Taylor/Newton approximations. If calc_math.c is ever made more precise
// these will change, and that is a deliberate improvement, not a regression.
static void test_precision_notes(CalcEngine *e) {
  group("known precision residues");

  // sqrt(2)^2 re-parses the 12-digit sqrt display, so it lands just under 2 at
  // the engine level. On the watch the UI's repeating-tail shortener
  // (prv_shorten_repeating in calc_ui.c) collapses the 9s and it reads as "2";
  // this asserts the raw engine string.
  reset_std(e); FEED(e, DIGIT_2, SQRT, SQUARE);   ASSERT_DISPLAY(e, "1.99999999999"); // true 2

  // Trig near-zero noise is NOT here anymore — prv_snap_zero() cleans it up; see
  // the cos(90)/sin(180)/tan(180) assertions in test_trig().

  // The snap must never swallow a legitimately small value. These paths don't
  // route through the trig snap, so they keep full range.
  reset_std(e); FEED(e, DIGIT_1, EE, DIGIT_1, DIGIT_5, NEGATE); feed(e, "*1=");
  ASSERT_DISPLAY(e, "1e-15");
}

// ===========================================================================
// RPN mode
// ===========================================================================

static void test_rpn_basic(CalcEngine *e) {
  group("rpn basic");

  reset_rpn(e); FEED(e, DIGIT_3, ENTER, DIGIT_4, ADD);         ASSERT_DISPLAY(e, "7");
  reset_rpn(e); FEED(e, DIGIT_1, DIGIT_0, ENTER, DIGIT_3, SUB); ASSERT_DISPLAY(e, "7");  // Y-X
  reset_rpn(e); FEED(e, DIGIT_1, DIGIT_0, ENTER, DIGIT_4, DIV); ASSERT_DISPLAY(e, "2.5"); // Y/X
  reset_rpn(e); FEED(e, DIGIT_6, ENTER, DIGIT_7, MUL);         ASSERT_DISPLAY(e, "42");
}

static void test_rpn_stack_ops(CalcEngine *e) {
  group("rpn stack ops");

  reset_rpn(e); FEED(e, DIGIT_3, ENTER, DIGIT_4, SWAP);
  ASSERT_DISPLAY(e, "3"); ASSERT_STACK(e, 2, "4");

  reset_rpn(e); FEED(e, DIGIT_5, ENTER, DIGIT_6, ADD, DIGIT_2, MUL); ASSERT_DISPLAY(e, "22");
  reset_rpn(e); FEED(e, DIGIT_8, ENTER, DIGIT_9, DROP);              ASSERT_DISPLAY(e, "8");
  reset_rpn(e); FEED(e, DIGIT_5, ENTER, DIGIT_3, ADD, LAST_X);       ASSERT_DISPLAY(e, "3");
}

static void test_rpn_chains(CalcEngine *e) {
  group("rpn complex chains");

  reset_rpn(e); FEED(e, DIGIT_3, ENTER, DIGIT_4, ENTER, DIGIT_5, MUL, ADD); ASSERT_DISPLAY(e, "23"); // 3+(4*5)
  reset_rpn(e); FEED(e, DIGIT_2, ENTER, DIGIT_3, ENTER, DIGIT_4, ADD, MUL); ASSERT_DISPLAY(e, "14"); // 2*(3+4)
  reset_rpn(e); FEED(e, DIGIT_8, ENTER, DIGIT_2, DIV, DIGIT_3, SUB);        ASSERT_DISPLAY(e, "1");  // 8/2-3
  reset_rpn(e); FEED(e, DIGIT_5, ENTER, DIGIT_1, DIGIT_0, ENTER, DIGIT_3, MUL, ADD); ASSERT_DISPLAY(e, "35"); // 5+(10*3)
  reset_rpn(e); FEED(e, DIGIT_2, ENTER, DIGIT_1, DIGIT_0, POW);            ASSERT_DISPLAY(e, "1024"); // 2^10
}

static void test_rpn_errors(CalcEngine *e) {
  group("rpn errors");

  reset_rpn(e); FEED(e, DIGIT_5, ENTER, DIGIT_0, DIV);   ASSERT_DISPLAY(e, "Error");
}

// ===========================================================================
// Formatting boundaries (calc_format_double directly)
// ===========================================================================

static void test_format(CalcEngine *e) {
  group("format boundaries");
  (void)e;

  char buf[CALC_FORMAT_BUF_SIZE];
  bool err;

  err = false; calc_format_double(1e308, buf, &err);
  check_eq(__FILE__, __LINE__, buf, "Error");
  check_eq(__FILE__, __LINE__, err ? "err" : "ok", "err");

  err = false; calc_format_double(0.5, buf, &err);      check_eq(__FILE__, __LINE__, buf, "0.5");
  err = false; calc_format_double(-42.0, buf, &err);    check_eq(__FILE__, __LINE__, buf, "-42");
  err = false; calc_format_double(1e40, buf, &err);     check_eq(__FILE__, __LINE__, buf, "1e40");
  err = false; calc_format_double(0.0, buf, &err);      check_eq(__FILE__, __LINE__, buf, "0");
  check_eq(__FILE__, __LINE__, err ? "err" : "ok", "ok");
}

int main(void) {
  CalcEngine engine;
  CalcEngine *e = &engine;

  // Standard mode
  test_infix_basic(e);
  test_infix_precedence(e);
  test_infix_chains(e);
  test_infix_entry_editing(e);
  test_infix_errors(e);
  test_infix_secondary(e);
  test_infix_clear(e);
  test_percent(e);

  // Scientific
  test_trig(e);
  test_inverse_trig(e);
  test_log_exp(e);
  test_powers_roots(e);
  test_factorial(e);
  test_hms(e);
  test_constants(e);
  test_second_modifier(e);
  test_exponent_entry(e);
  test_precision_notes(e);

  // RPN
  test_rpn_basic(e);
  test_rpn_stack_ops(e);
  test_rpn_chains(e);
  test_rpn_errors(e);
  test_rpn_clear(e);

  // Formatting
  test_format(e);

  return report();
}
