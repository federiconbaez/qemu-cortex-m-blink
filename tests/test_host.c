/* SPDX-License-Identifier: Apache-2.0
 * Host tests for the Morse compiler and player (built with ASan + UBSan). */
#include <stdio.h>
#include <string.h>

#include "morse.h"

static int failures;
#define CHECK(c)                                                       \
  do {                                                                 \
    if (!(c)) {                                                        \
      fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #c); \
      ++failures;                                                      \
    }                                                                  \
  } while (0)

/* Independent re-statement of the ITU table as one string, so a typo in
 * morse.c cannot be copied into the expectation. */
static void test_table(void) {
  const char *expect =
      "A.- B-... C-.-. D-.. E. F..-. G--. H.... I.. J.--- K-.- L.-.. M-- N-. O--- P.--. Q--.- R.-. S... T- U..- "
      "V...- W.-- X-..- Y-.-- Z--.. 0----- 1.---- 2..--- 3...-- 4....- 5..... 6-.... 7--... 8---.. 9----.";
  char got[512] = {0};
  const char *chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  for (const char *c = chars; *c; ++c) {
    char item[16];
    snprintf(item, sizeof item, "%s%c%s", c == chars ? "" : " ", *c, morse_pattern(*c));
    strcat(got, item);
  }
  CHECK(strcmp(got, expect) == 0);
  CHECK(strcmp(morse_pattern('q'), morse_pattern('Q')) == 0);
  CHECK(morse_pattern('?') == NULL && morse_pattern(' ') == NULL && morse_pattern('\0') == NULL);
  /* No two characters share a pattern (the code is uniquely decodable per letter). */
  for (const char *a = chars; *a; ++a)
    for (const char *b = a + 1; *b; ++b) CHECK(strcmp(morse_pattern(*a), morse_pattern(*b)) != 0);
}

static void test_paris(void) {
  /* "PARIS " is the standard word: exactly 50 units. */
  morse_step s[64];
  int n = morse_compile("PARIS ", s, 64, NULL);
  CHECK(n > 0);
  CHECK(morse_units(s, (size_t)n) == 50);
  /* Steps alternate on/off, start on, end with the 7-unit word gap. */
  for (int i = 0; i < n; ++i) CHECK(s[i].on == (i % 2 == 0));
  CHECK(s[n - 1].on == 0 && s[n - 1].units == 7);
  /* Without the trailing space: 43 units, ends on. */
  n = morse_compile("paris", s, 64, NULL);
  CHECK(morse_units(s, (size_t)n) == 43 && s[n - 1].on == 1);
}

static void test_spacing(void) {
  morse_step a[32], b[32];
  int na = morse_compile("E E", a, 32, NULL);
  int nb = morse_compile("   E    E   ", b, 32, NULL);
  /* E E: on1 off7 on1 */
  CHECK(na == 3 && a[0].units == 1 && a[1].units == 7 && a[1].on == 0 && a[2].units == 1);
  /* leading spaces ignored, runs collapse, trailing adds one word gap */
  CHECK(nb == 4 && memcmp(a, b, 3 * sizeof *a) == 0 && b[3].on == 0 && b[3].units == 7);
  CHECK(morse_compile("", a, 32, NULL) == 0);
  CHECK(morse_compile("    ", a, 32, NULL) == 0);
  /* T then E inside a word: on3 off3 on1 */
  CHECK(morse_compile("TE", a, 32, NULL) == 3 && a[0].units == 3 && a[1].units == 3 && a[2].units == 1);
}

static void test_errors(void) {
  morse_step s[32];
  size_t bad = 99;
  CHECK(morse_compile("SO?S", s, 32, &bad) == MORSE_EBADCHAR && bad == 2);
  CHECK(morse_compile("\xC3\xB1", s, 32, &bad) == MORSE_EBADCHAR && bad == 0);
  /* "SOS" needs 5+1+... = 17 steps; every capacity below that must fail cleanly */
  morse_step big[32];
  int need = morse_compile("SOS", big, 32, NULL);
  CHECK(need == 17);
  for (int cap = 0; cap < need; ++cap) {
    morse_step tmp[17];
    CHECK(morse_compile("SOS", tmp, (size_t)cap, NULL) == MORSE_ENOSPC);
  }
  morse_step tmp[17];
  CHECK(morse_compile("SOS", tmp, 17, NULL) == 17);
}

/* The exact edge timeline the firmware must reproduce on the GPIO pin:
 * "SOS " at 2 ticks per unit, edges as (tick, level). Derived by hand:
 * S = on 0-2, 4-6, 8-10; gap 3u -> O = on 16-22, 24-30, 32-38; gap 3u ->
 * S = on 44-46, 48-50, 52-54; word gap 7u -> next loop starts at 68. */
static const unsigned golden[][2] = {
    {0, 1},  {2, 0},  {4, 1},  {6, 0},  {8, 1},  {10, 0}, {16, 1}, {22, 0}, {24, 1}, {30, 0}, {32, 1}, {38, 0},
    {44, 1}, {46, 0}, {48, 1}, {50, 0}, {52, 1}, {54, 0}, {68, 1}, {70, 0},
};

static void test_player_timeline(void) {
  morse_step s[32];
  int n = morse_compile("SOS ", s, 32, NULL);
  CHECK(morse_units(s, (size_t)n) == 34);
  morse_player p;
  morse_player_init(&p, s, (size_t)n, 2, 1);
  unsigned edges[64][2];
  size_t ne = 0;
  int prev = 0;
  for (unsigned t = 0; t < 72; ++t) {
    int lv = morse_player_tick(&p);
    if (lv != prev && ne < 64) {
      edges[ne][0] = t;
      edges[ne][1] = (unsigned)lv;
      ++ne;
    }
    prev = lv;
  }
  CHECK(ne == sizeof golden / sizeof golden[0]);
  CHECK(memcmp(edges, golden, sizeof golden) == 0);

  /* Non-looping: exactly units*tpu ticks of output, then done and dark. */
  morse_player_init(&p, s, (size_t)n, 3, 0);
  unsigned on_ticks = 0, ticks = 0;
  while (!p.done) {
    on_ticks += (unsigned)morse_player_tick(&p);
    ++ticks;
  }
  CHECK(ticks == 34 * 3);
  CHECK(on_ticks == (1 + 1 + 1 + 3 + 3 + 3 + 1 + 1 + 1) * 3);
  CHECK(morse_player_tick(&p) == 0);

  morse_player_init(&p, s, 0, 1, 1);
  CHECK(p.done && morse_player_tick(&p) == 0);
}

int main(void) {
  test_table();
  test_paris();
  test_spacing();
  test_errors();
  test_player_timeline();
  if (failures) {
    fprintf(stderr, "%d failure(s)\n", failures);
    return 1;
  }
  puts("host tests: all checks passed");
  return 0;
}
