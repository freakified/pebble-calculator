#pragma once

#include <stdbool.h>

// Buffer size for any formatted display string (must hold 13 digits + sign +
// dot + null, or scientific notation up to CALC_FORMAT_SCI_MAX chars).
#define CALC_FORMAT_BUF_SIZE 17  // CALC_FORMAT_MAX_PLAIN + 1

// Plain-format width: max digit characters (0-9) before scientific notation
// kicks in. The minus sign DOES consume one digit slot; the decimal point
// does not.
#define CALC_FORMAT_MAX_DIGITS 13

// Scientific-notation width: target maximum characters for [-]D.DDDeDD form.
#define CALC_FORMAT_SCI_MAX 10

// Parse a numeric string (plain or scientific) into a double.
// Accepts leading '-', optional fractional part, optional 'eNN' exponent.
double calc_format_parse(const char *str, int len);

// Format a double into buf. Returns characters written (excluding the null).
// buf must be at least CALC_FORMAT_BUF_SIZE bytes.
// On values too large to display, writes "Error" and (if non-NULL) sets
// *out_error = true. Pass NULL when the caller doesn't care.
int calc_format_double(double val, char *buf, bool *out_error);
