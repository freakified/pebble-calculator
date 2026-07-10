#pragma once

#include <stdbool.h>

// Buffer size for any formatted display string (must hold 13 digits + sign +
// dot + null, or scientific notation up to CALC_FORMAT_SCI_MAX chars).
#define CALC_FORMAT_BUF_SIZE 17  // CALC_FORMAT_MAX_PLAIN + 1

// Plain-format width: max digit characters (0-9) before scientific notation
// kicks in. The minus sign DOES consume one digit slot; the decimal point
// does not.
//
// Derived from the compact display font so a formatted string always fits
// without the renderer's trailing ellipsis: 12 digits + dot = 137px and
// 11 digits + minus + dot = 134px, both within CALC_FORMAT_DISPLAY_W.
#define CALC_FORMAT_MAX_DIGITS 12

// Pixel-width model of the compact display font (Gothic 28 Bold), measured on
// emery and gabbro firmware: every digit and 'e' advance 11px, '-' 8px,
// '.' 5px. The display text area is 138px wide on both platforms (the display
// rect is 144px with 6px right padding). Entry caps in the engine use this
// model so a string the user can type always fits the compact font.
#define CALC_FORMAT_DISPLAY_W 138
#define CALC_FORMAT_W_DIGIT 11  // '0'-'9' and 'e'
#define CALC_FORMAT_W_MINUS 8
#define CALC_FORMAT_W_DOT 5

// Scientific-notation width: target maximum characters for [-]D.DDDeDD form.
#define CALC_FORMAT_SCI_MAX 10

// Largest magnitude the formatter can display; anything beyond (or NaN) is
// "Error". The engine's overflow detection uses the same boundary so a value
// it accepts always renders.
#define CALC_FORMAT_MAX_ABS 9.9999e307

// Parse a numeric string (plain or scientific) into a double.
// Accepts leading '-', optional fractional part, optional 'eNN' exponent.
double calc_format_parse(const char *str, int len);

// Pixel width of str in the compact display font per the model above.
int calc_format_display_width(const char *str, int len);

// Format a double into buf. Returns characters written (excluding the null).
// buf must be at least CALC_FORMAT_BUF_SIZE bytes.
// On values too large to display, writes "Error" and (if non-NULL) sets
// *out_error = true. Pass NULL when the caller doesn't care.
int calc_format_double(double val, char *buf, bool *out_error);
