// Test sanitizer_common interception on `mbrtowc()`.
//
// RUN: %clang -O0 -g %s -o %t && %run %t

#include <assert.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static int set_utf8_locale(void) {
  return (setlocale(LC_CTYPE, "C.UTF-8") ||
          setlocale(LC_CTYPE, "en_US.UTF-8") ||
          setlocale(LC_CTYPE, "en_US.utf8")) &&
         MB_CUR_MAX >= 4;
}

static void test_mbrtowc_ascii(void) {
  mbstate_t st;
  memset(&st, 0, sizeof(st));
  wchar_t wc = L'x';
  assert(mbrtowc(&wc, "a", 1, &st) == 1);
  assert(wc == L'a');
}

static void test_mbrtowc_null_char(void) {
  mbstate_t st;
  memset(&st, 0, sizeof(st));
  wchar_t wc = L'x';
  assert(mbrtowc(&wc, "", 1, &st) == 0);
  assert(wc == L'\0');
}

static void test_mbrtowc_null_pwc(void) {
  mbstate_t st;
  memset(&st, 0, sizeof(st));
  assert(mbrtowc(NULL, "b", 1, &st) == 1);
}

static void test_mbrtowc_null_src(void) {
  mbstate_t st;
  memset(&st, 0, sizeof(st));
  wchar_t wc = L'x';
  assert(mbrtowc(&wc, NULL, 10, &st) == 0);
  assert(wc == L'x');
}

static void test_mbrtowc_zero_length(void) {
  mbstate_t st;
  memset(&st, 0, sizeof(st));
  wchar_t wc = L'x';
  assert(mbrtowc(&wc, "a", 0, &st) == (size_t)-2);
  assert(wc == L'x');
}

static void test_mbrtowc_utf8_multibyte(void) {
  if (!set_utf8_locale())
    return;

  const char euro[3] = {'\xe2', '\x82', '\xac'};
  mbstate_t st;
  memset(&st, 0, sizeof(st));
  wchar_t wc = 0;
  assert(mbrtowc(&wc, euro, sizeof(euro), &st) == 3);
  assert(wc == L'\u20ac');
}

static void test_mbrtowc_utf8_incomplete(void) {
  if (!set_utf8_locale())
    return;

  const char euro[3] = {'\xe2', '\x82', '\xac'};
  mbstate_t st;
  memset(&st, 0, sizeof(st));
  wchar_t wc = L'x';
  assert(mbrtowc(&wc, euro, 1, &st) == (size_t)-2);
  assert(wc == L'x');
  assert(mbrtowc(&wc, euro + 1, 1, &st) == (size_t)-2);
  assert(wc == L'x');
  assert(mbrtowc(&wc, euro + 2, 1, &st) == 1);
  assert(wc == L'\u20ac');
}

static void test_mbrtowc_invalid_seq(void) {
  if (!set_utf8_locale())
    return;

  mbstate_t st;
  memset(&st, 0, sizeof(st));
  wchar_t wc = L'x';
  assert(mbrtowc(&wc, "\xff", 1, &st) == (size_t)-1);
  assert(wc == L'x');
}

int main(void) {
  test_mbrtowc_ascii();
  test_mbrtowc_null_char();
  test_mbrtowc_null_pwc();
  test_mbrtowc_null_src();
  test_mbrtowc_zero_length();
  test_mbrtowc_utf8_multibyte();
  test_mbrtowc_utf8_incomplete();
  test_mbrtowc_invalid_seq();
  return 0;
}
