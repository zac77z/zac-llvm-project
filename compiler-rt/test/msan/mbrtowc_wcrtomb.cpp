// Test MSan interception on `mbrtowc()` and `wcrtomb()`.
//
// RUN: %clangxx_msan -O0 -g %s -o %t
// RUN: %run %t
// RUN: not %run %t src 2>&1 | FileCheck %s
// RUN: not %run %t state 2>&1 | FileCheck %s
// RUN: not %run %t wcrtomb_state 2>&1 | FileCheck %s

#include <assert.h>
#include <locale.h>
#include <sanitizer/msan_interface.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static bool set_utf8_locale() {
  return (setlocale(LC_CTYPE, "C.UTF-8") ||
          setlocale(LC_CTYPE, "en_US.UTF-8") ||
          setlocale(LC_CTYPE, "en_US.utf8")) &&
         MB_CUR_MAX >= 4;
}

static void test_uninit_src() {
  mbstate_t st = {};
  char src[4];
  __msan_poison(src, sizeof(src));
  wchar_t wc;
  (void)mbrtowc(&wc, src, sizeof(src), &st);
}

static void test_uninit_state() {
  mbstate_t st;
  __msan_poison(&st, sizeof(st));
  wchar_t wc;
  (void)mbrtowc(&wc, "a", 1, &st);
}

static void test_wcrtomb_uninit_state() {
  mbstate_t st;
  __msan_poison(&st, sizeof(st));
  char buf[16];
  (void)wcrtomb(buf, L'a', &st);
}

// Trailing uninitialized bytes past a complete character must not fault.
static void test_mbrtowc_partial_init() {
  mbstate_t st = {};
  char partial_init[8];
  __msan_poison(partial_init, sizeof(partial_init));
  partial_init[0] = 'x';

  wchar_t wc;
  __msan_poison(&wc, sizeof(wc));
  assert(mbrtowc(&wc, partial_init, sizeof(partial_init), &st) == 1);
  assert(__msan_test_shadow(&wc, sizeof(wc)) == -1);
  assert(wc == L'x');

  partial_init[0] = '\0';
  __msan_poison(&wc, sizeof(wc));
  assert(mbrtowc(&wc, partial_init, sizeof(partial_init), &st) == 0);
  assert(__msan_test_shadow(&wc, sizeof(wc)) == -1);
  assert(wc == L'\0');
}

// s == nullptr must not write or unpoison *pwc.
static void test_mbrtowc_null_src() {
  mbstate_t st = {};
  wchar_t wc;
  __msan_poison(&wc, sizeof(wc));
  assert(mbrtowc(&wc, nullptr, 4, &st) == 0);
  assert(__msan_test_shadow(&wc, sizeof(wc)) == 0);
}

// Incomplete sequence (-2) with n == 0 must leave *pwc poisoned.
static void test_mbrtowc_zero_length() {
  mbstate_t st = {};
  wchar_t wc;
  __msan_poison(&wc, sizeof(wc));
  assert(mbrtowc(&wc, "a", 0, &st) == (size_t)-2);
  assert(__msan_test_shadow(&wc, sizeof(wc)) == 0);
}

// Encoding error (-1) must leave *pwc poisoned.
static void test_mbrtowc_invalid_seq() {
  if (!set_utf8_locale())
    return;

  mbstate_t st = {};
  wchar_t wc;
  __msan_poison(&wc, sizeof(wc));
  assert(mbrtowc(&wc, "\xff", 1, &st) >= (size_t)-2);
  assert(__msan_test_shadow(&wc, sizeof(wc)) == 0);
}

// Incomplete multibyte sequence (-2) leaves *pwc poisoned until completed.
static void test_mbrtowc_utf8_incomplete() {
  if (!set_utf8_locale())
    return;

  const char euro[3] = {'\xe2', '\x82', '\xac'};
  mbstate_t st = {};
  wchar_t wc;
  __msan_poison(&wc, sizeof(wc));
  assert(mbrtowc(&wc, euro, 1, &st) == (size_t)-2);
  assert(__msan_test_shadow(&wc, sizeof(wc)) == 0);
  assert(mbrtowc(&wc, euro + 1, 1, &st) == (size_t)-2);
  assert(__msan_test_shadow(&wc, sizeof(wc)) == 0);
  assert(mbrtowc(&wc, euro + 2, 1, &st) == 1);
  assert(__msan_test_shadow(&wc, sizeof(wc)) == -1);
  assert(wc == L'\u20ac');
}

// wcrtomb unpoisons only the written bytes.
static void test_wcrtomb_ascii() {
  mbstate_t st = {};
  char buf[16];
  __msan_poison(buf, sizeof(buf));
  assert(wcrtomb(buf, L'q', &st) == 1);
  assert(__msan_test_shadow(buf, 1) == -1);
  assert(__msan_test_shadow(buf + 1, 1) == 0);
  assert(buf[0] == 'q');
}

static void test_wcrtomb_utf8_multibyte() {
  if (!set_utf8_locale())
    return;

  mbstate_t st = {};
  char buf[16];
  __msan_poison(buf, sizeof(buf));
  assert(wcrtomb(buf, L'\u20ac', &st) == 3);
  assert(__msan_test_shadow(buf, 3) == -1);
  assert(__msan_test_shadow(buf + 3, 1) == 0);
  assert(memcmp(buf, "\xe2\x82\xac", 3) == 0);
}

int main(int argc, char **argv) {
  if (argc > 1) {
    if (strcmp(argv[1], "src") == 0) {
      test_uninit_src();
    } else if (strcmp(argv[1], "state") == 0) {
      test_uninit_state();
    } else if (strcmp(argv[1], "wcrtomb_state") == 0) {
      test_wcrtomb_uninit_state();
    }
    // CHECK: MemorySanitizer: use-of-uninitialized-value
    return 0;
  }

  test_mbrtowc_partial_init();
  test_mbrtowc_null_src();
  test_mbrtowc_zero_length();
  test_mbrtowc_invalid_seq();
  test_mbrtowc_utf8_incomplete();
  test_wcrtomb_ascii();
  test_wcrtomb_utf8_multibyte();
  return 0;
}
