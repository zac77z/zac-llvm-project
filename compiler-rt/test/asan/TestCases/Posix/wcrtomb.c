// Test ASan interception on `wcrtomb()`.
//
// Function not intercepted on android.
// UNSUPPORTED: android
// RUN: %clangxx_asan -O0 -g %s -o %t
// RUN: %run %t
// RUN: not %run %t dest 2>&1 | FileCheck %s
// RUN: not %run %t state 2>&1 | FileCheck %s
// RUN: not %run %t null_dest_state 2>&1 | FileCheck %s

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

static void test_wcrtomb_dest_uaf(void) {
  char *buff = (char *)malloc(MB_CUR_MAX);
  free(buff);
  wcrtomb(buff, L'a', NULL);
}

static void test_wcrtomb_state_uaf(void) {
  mbstate_t *st = (mbstate_t *)malloc(sizeof(mbstate_t));
  memset(st, 0, sizeof(mbstate_t));
  free(st);
  char buff[16];
  wcrtomb(buff, L'a', st);
}

static void test_wcrtomb_null_dest_state_uaf(void) {
  mbstate_t *st = (mbstate_t *)malloc(sizeof(mbstate_t));
  memset(st, 0, sizeof(mbstate_t));
  free(st);
  wcrtomb(NULL, L'\0', st);
}

static void test_wcrtomb_short_alloc(void) {
  mbstate_t st;
  memset(&st, 0, sizeof(st));
  char *one_byte = (char *)malloc(1);
  assert(wcrtomb(one_byte, L'a', &st) == 1);
  assert(one_byte[0] == 'a');
  assert(wcrtomb(one_byte, L'\0', &st) == 1);
  assert(one_byte[0] == '\0');
  free(one_byte);
}

static void test_wcrtomb_null_dest(void) {
  mbstate_t st;
  memset(&st, 0, sizeof(st));
  assert(wcrtomb(NULL, L'\0', &st) == 1);
}

static void test_wcrtomb_utf8_exact_alloc(void) {
  if (!set_utf8_locale())
    return;

  mbstate_t st;
  memset(&st, 0, sizeof(st));
  char *three_bytes = (char *)malloc(3);
  assert(wcrtomb(three_bytes, L'\u20ac', &st) == 3);
  assert(memcmp(three_bytes, "\xe2\x82\xac", 3) == 0);
  free(three_bytes);
}

int main(int argc, char **argv) {
  if (argc > 1) {
    if (strcmp(argv[1], "dest") == 0) {
      test_wcrtomb_dest_uaf();
    } else if (strcmp(argv[1], "state") == 0) {
      test_wcrtomb_state_uaf();
    } else if (strcmp(argv[1], "null_dest_state") == 0) {
      test_wcrtomb_null_dest_state_uaf();
    }
    // CHECK: heap-use-after-free
    // CHECK: SUMMARY
    return 0;
  }

  test_wcrtomb_short_alloc();
  test_wcrtomb_null_dest();
  test_wcrtomb_utf8_exact_alloc();
  return 0;
}
