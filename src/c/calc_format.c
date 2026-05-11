#include "calc_format.h"
#include <stdio.h>
#include <string.h>

double calc_format_parse(const char *str, int len) {
  double result = 0.0;
  bool negative = false;
  int i = 0;

  if (len > 0 && str[0] == '-') {
    negative = true;
    i = 1;
  }

  for (; i < len && str[i] != '.' && str[i] != 'e'; i++) {
    result = result * 10.0 + (double)(str[i] - '0');
  }

  if (i < len && str[i] == '.') {
    i++;
    double frac = 0.1;
    for (; i < len && str[i] != 'e'; i++) {
      result += (double)(str[i] - '0') * frac;
      frac *= 0.1;
    }
  }

  if (i < len && str[i] == 'e') {
    i++;
    bool exp_neg = false;
    if (i < len && str[i] == '-') {
      exp_neg = true;
      i++;
    } else if (i < len && str[i] == '+') {
      i++;
    }
    int exp = 0;
    for (; i < len; i++) {
      exp = exp * 10 + (str[i] - '0');
    }
    if (exp_neg) {
      for (int j = 0; j < exp; j++) result /= 10.0;
    } else {
      for (int j = 0; j < exp; j++) result *= 10.0;
    }
  }

  return negative ? -result : result;
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

  int first = (int)mantissa;
  if (first > 9) first = 9;
  buf[pos++] = '0' + (char)first;
  mantissa -= (double)first;

  // Reserve space for the exponent suffix
  int abs_exp = exp < 0 ? -exp : exp;
  int exp_chars = 1; // 'e'
  if (exp < 0) exp_chars++; // '-'
  if (abs_exp >= 100) exp_chars += 3;
  else if (abs_exp >= 10) exp_chars += 2;
  else exp_chars += 1;

  int avail = CALC_FORMAT_SCI_MAX - pos - exp_chars - 1; // -1 for '.'
  if (avail < 0) avail = 0;

  if (avail > 0 && mantissa > 0.0000001) {
    buf[pos++] = '.';
    for (int d = 0; d < avail; d++) {
      mantissa *= 10.0;
      int digit = (int)mantissa;
      if (digit > 9) digit = 9;
      buf[pos++] = '0' + (char)digit;
      mantissa -= (double)digit;
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

  // True error: beyond useful double range (e.g. div-by-zero overflow)
  if (val > 9.999e15) {
    memcpy(buf, "Error", 6);
    if (out_error) *out_error = true;
    return 5;
  }

  int max_digits = negative ? (CALC_FORMAT_MAX_DIGITS - 1) : CALC_FORMAT_MAX_DIGITS;

  long long int_part = (long long)val;
  int int_digit_count = 0;
  {
    long long n = int_part;
    if (n == 0) {
      int_digit_count = 1;
    } else {
      while (n > 0) { int_digit_count++; n /= 10; }
    }
  }

  if (int_digit_count > max_digits) {
    return prv_format_scientific(val, buf, negative);
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

  double frac_part = val - (double)int_part;
  int max_frac = max_digits - int_digit_count;

  if (frac_part > 0.0000000005 && max_frac > 0) {
    char frac_digits[10];
    int frac_len = 0;
    double f = frac_part;
    for (int d = 0; d < max_frac && d < 9; d++) {
      f *= 10.0;
      int digit = (int)f;
      if (digit > 9) digit = 9;
      frac_digits[frac_len++] = '0' + (char)digit;
      f -= (double)digit;
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
