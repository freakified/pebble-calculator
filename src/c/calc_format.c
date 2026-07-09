#include "calc_format.h"
#include <stdio.h>
#include <string.h>

double calc_format_parse(const char *str, int len) {
  // Accumulate the mantissa digits as one exact integer (display caps keep it
  // well under 2^53) and apply the combined decimal exponent with a single
  // multiply or divide, so the result is the correctly-rounded double rather
  // than a sum of inexact 10^-k terms.
  double mantissa = 0.0;
  int dec_places = 0;
  bool negative = false;
  bool seen_dot = false;
  int i = 0;

  if (len > 0 && str[0] == '-') {
    negative = true;
    i = 1;
  }

  for (; i < len && str[i] != 'e'; i++) {
    if (str[i] == '.') {
      seen_dot = true;
      continue;
    }
    mantissa = mantissa * 10.0 + (double)(str[i] - '0');
    if (seen_dot) dec_places++;
  }

  int exp = 0;
  bool exp_neg = false;
  if (i < len && str[i] == 'e') {
    i++;
    if (i < len && str[i] == '-') {
      exp_neg = true;
      i++;
    } else if (i < len && str[i] == '+') {
      i++;
    }
    for (; i < len; i++) {
      exp = exp * 10 + (str[i] - '0');
    }
  }

  int total_exp = (exp_neg ? -exp : exp) - dec_places;
  double result = mantissa;
  if (total_exp != 0) {
    int n = total_exp < 0 ? -total_exp : total_exp;
    double p = 1.0;
    for (int j = 0; j < n; j++) p *= 10.0; // overflows to inf for huge n; /inf → 0
    result = (total_exp < 0) ? mantissa / p : mantissa * p;
  }

  return negative ? -result : result;
}

static int prv_exp_suffix_chars(int exp) {
  int abs_exp = exp < 0 ? -exp : exp;
  int chars = 1; // 'e'
  if (exp < 0) chars++; // '-'
  if (abs_exp >= 100) chars += 3;
  else if (abs_exp >= 10) chars += 2;
  else chars += 1;
  return chars;
}

// Format val (already non-negative) into buf in scientific notation. Returns
// chars written. negative is prepended as a leading '-'.
static int prv_format_scientific(double val, char *buf, bool negative) {
  int pos = 0;

  int exp = 0;
  double mantissa = val;
  if (val >= 10.0) {
    while (mantissa >= 10.0) { mantissa /= 10.0; exp++; }
  } else if (val > 0.0 && val < 1.0) {
    while (mantissa < 1.0) { mantissa *= 10.0; exp--; }
  }

  if (negative) buf[pos++] = '-';

  // Fraction digits that fit after the leading digit, '.', and exponent suffix.
  int avail = CALC_FORMAT_SCI_MAX - pos - 1 - 1 - prv_exp_suffix_chars(exp);
  if (avail < 0) avail = 0;

  // Round the whole mantissa at the last displayed digit.
  long long scale = 1;
  for (int i = 0; i < avail; i++) scale *= 10;
  long long m = (long long)(mantissa * (double)scale + 0.5);
  if (m >= 10 * scale) {
    // 9.99… rounded up to 10.0 — carry into the exponent. The carried
    // mantissa is exactly 1.000…, so if the wider exponent costs a fraction
    // digit the trailing zeros absorb it.
    exp++;
    avail = CALC_FORMAT_SCI_MAX - pos - 1 - 1 - prv_exp_suffix_chars(exp);
    if (avail < 0) avail = 0;
    scale = 1;
    for (int i = 0; i < avail; i++) scale *= 10;
    m = scale;
  }

  buf[pos++] = '0' + (char)(m / scale);
  long long frac = m % scale;

  if (frac > 0) {
    buf[pos++] = '.';
    long long div = scale / 10;
    for (int d = 0; d < avail; d++) {
      buf[pos++] = '0' + (char)((frac / div) % 10);
      div /= 10;
    }
    while (pos > 0 && buf[pos - 1] == '0') pos--;
    if (pos > 0 && buf[pos - 1] == '.') pos--;
  }

  buf[pos++] = 'e';
  if (exp < 0) {
    buf[pos++] = '-';
    exp = -exp;
  }
  if (exp >= 100) {
    buf[pos++] = '0' + (char)(exp / 100);
    buf[pos++] = '0' + (char)((exp / 10) % 10);
    buf[pos++] = '0' + (char)(exp % 10);
  } else if (exp >= 10) {
    buf[pos++] = '0' + (char)(exp / 10);
    buf[pos++] = '0' + (char)(exp % 10);
  } else {
    buf[pos++] = '0' + (char)exp;
  }

  buf[pos] = '\0';
  return pos;
}

int calc_format_double(double val, char *buf, bool *out_error) {
  if (out_error) *out_error = false;

  bool negative = false;
  if (val < 0.0) {
    negative = true;
    val = -val;
  }

  // True error: NaN or beyond double's representable range.
  if (val != val || val > 9.9999e307) {
    memcpy(buf, "Error", 6);
    if (out_error) *out_error = true;
    return 5;
  }

  int max_digits = negative ? (CALC_FORMAT_MAX_DIGITS - 1) : CALC_FORMAT_MAX_DIGITS;

  // Too many integer digits for the plain format — scientific. (Also keeps
  // the long long cast below well in range.)
  if (val >= (negative ? 1e12 : 1e13)) {
    return prv_format_scientific(val, buf, negative);
  }

  long long int_part = (long long)val;
  double frac_part = val - (double)int_part;

  int int_digit_count = 0;
  {
    long long n = int_part;
    if (n == 0) {
      int_digit_count = 1;
    } else {
      while (n > 0) { int_digit_count++; n /= 10; }
    }
  }

  // Round the fraction at the last displayed digit (this is what keeps
  // 1.2 + 1.2 from showing as 2.399999999).
  int max_frac = max_digits - int_digit_count;
  if (max_frac < 0) max_frac = 0;
  long long frac_scale = 1;
  for (int i = 0; i < max_frac; i++) frac_scale *= 10;
  long long frac_scaled = (long long)(frac_part * (double)frac_scale + 0.5);
  if (frac_scaled >= frac_scale) {
    // Fraction rounded up to 1.0 — carry into the integer part.
    frac_scaled = 0;
    int_part++;
    int_digit_count = 0;
    long long n = int_part;
    while (n > 0) { int_digit_count++; n /= 10; }
    if (int_digit_count > max_digits) {
      return prv_format_scientific(val, buf, negative);
    }
  }

  char tmp_buf[CALC_FORMAT_BUF_SIZE];
  int pos = 0;

  if (int_part == 0) {
    tmp_buf[pos++] = '0';
  } else {
    char tmp[16];
    int tmp_len = 0;
    long long n = int_part;
    while (n > 0 && tmp_len < 15) {
      tmp[tmp_len++] = '0' + (char)(n % 10);
      n /= 10;
    }
    for (int j = tmp_len - 1; j >= 0; j--) {
      tmp_buf[pos++] = tmp[j];
    }
  }

  if (frac_scaled > 0) {
    char frac_digits[16];
    int frac_len = 0;
    long long div = frac_scale / 10;
    for (int d = 0; d < max_frac; d++) {
      frac_digits[frac_len++] = '0' + (char)((frac_scaled / div) % 10);
      div /= 10;
    }

    while (frac_len > 0 && frac_digits[frac_len - 1] == '0') {
      frac_len--;
    }

    if (frac_len > 0) {
      tmp_buf[pos++] = '.';
      for (int j = 0; j < frac_len; j++) {
        tmp_buf[pos++] = frac_digits[j];
      }
    }
  }

  // Small nonzero values that rounded down to "0" — switch to scientific
  if (val > 0.0 && pos == 1 && tmp_buf[0] == '0') {
    return prv_format_scientific(val, buf, negative);
  }

  if (negative && !(pos == 1 && tmp_buf[0] == '0')) {
    buf[0] = '-';
    memcpy(buf + 1, tmp_buf, pos);
    buf[pos + 1] = '\0';
    return pos + 1;
  }

  memcpy(buf, tmp_buf, pos);
  buf[pos] = '\0';
  return pos;
}
