// Test ASan interception on `mbrtowc()`.
//
// Function not intercepted on android.
// UNSUPPORTED: android
// RUN: %clangxx_asan -O0 -g %s -o %t
// RUN: %run %t
// RUN: not %run %t pwc 2>&1 | FileCheck %s
// RUN: not %run %t src 2>&1 | FileCheck %s

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static void test_mbrtowc_pwc_uaf() {
  mbstate_t st = {};
  wchar_t *p = (wchar_t *)malloc(sizeof(wchar_t));
  free(p);
  mbrtowc(p, "a", 1, &st);
}

static void test_mbrtowc_src_uaf() {
  mbstate_t st = {};
  char *p = (char *)malloc(1);
  p[0] = 'a';
  free(p);
  wchar_t wc = 0;
  mbrtowc(&wc, p, 1, &st);
}

// Passing n larger than the 1-byte allocation is valid when 1 byte completes
// the character.
static void test_mbrtowc_short_alloc() {
  mbstate_t st = {};
  char *one_byte = (char *)malloc(1);
  one_byte[0] = 'a';
  wchar_t wc = 0;
  assert(mbrtowc(&wc, one_byte, 16, &st) == 1);
  assert(wc == L'a');
  one_byte[0] = '\0';
  assert(mbrtowc(&wc, one_byte, 16, &st) == 0);
  assert(wc == L'\0');
  free(one_byte);
}

static void test_mbrtowc_null_src() {
  mbstate_t st = {};
  wchar_t wc = L'x';
  assert(mbrtowc(&wc, nullptr, 16, &st) == 0);
  assert(wc == L'x');
}

int main(int argc, char **argv) {
  if (argc > 1) {
    if (strcmp(argv[1], "pwc") == 0) {
      test_mbrtowc_pwc_uaf();
    } else if (strcmp(argv[1], "src") == 0) {
      test_mbrtowc_src_uaf();
    }
    // CHECK: heap-use-after-free
    // CHECK: SUMMARY
    return 0;
  }

  test_mbrtowc_short_alloc();
  test_mbrtowc_null_src();
  return 0;
}
