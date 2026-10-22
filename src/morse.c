/* SPDX-License-Identifier: Apache-2.0 */
#include "morse.h"

static const char *const letters[26] = {
    ".-",   "-...", "-.-.", "-..",  ".",   "..-.", "--.",  "....", "..",   ".---", "-.-",  ".-..", "--",
    "-.",   "---",  ".--.", "--.-", ".-.", "...",  "-",    "..-",  "...-", ".--",  "-..-", "-.--", "--..",
};
static const char *const digits[10] = {
    "-----", ".----", "..---", "...--", "....-", ".....", "-....", "--...", "---..", "----.",
};

const char *morse_pattern(char c) {
  if (c >= 'a' && c <= 'z') return letters[c - 'a'];
  if (c >= 'A' && c <= 'Z') return letters[c - 'A'];
  if (c >= '0' && c <= '9') return digits[c - '0'];
  return NULL;
}

/* Appends one step. Gaps never repeat: a word gap replaces the letter gap. */
static int push(morse_step *out, size_t cap, size_t *n, uint8_t on, uint8_t units) {
  if (*n == cap) return MORSE_ENOSPC;
  out[*n].on = on;
  out[*n].units = units;
  ++*n;
  return 0;
}

int morse_compile(const char *text, morse_step *out, size_t cap, size_t *bad) {
  size_t n = 0;
  int have_letter = 0;   /* a letter was emitted and no gap follows it yet */
  int pending_word = 0;  /* spaces seen since the last letter */
  for (size_t i = 0; text[i] != '\0'; ++i) {
    if (text[i] == ' ') {
      if (have_letter) pending_word = 1;
      continue;
    }
    const char *pat = morse_pattern(text[i]);
    if (pat == NULL) {
      if (bad) *bad = i;
      return MORSE_EBADCHAR;
    }
    if (have_letter) {
      int rc = push(out, cap, &n, 0, pending_word ? 7 : 3);
      if (rc) return rc;
    }
    pending_word = 0;
    for (size_t k = 0; pat[k] != '\0'; ++k) {
      int rc;
      if (k > 0 && (rc = push(out, cap, &n, 0, 1))) return rc;
      if ((rc = push(out, cap, &n, 1, pat[k] == '-' ? 3 : 1))) return rc;
    }
    have_letter = 1;
  }
  if (pending_word) {
    int rc = push(out, cap, &n, 0, 7);
    if (rc) return rc;
  }
  return (int)n;
}

uint32_t morse_units(const morse_step *steps, size_t n) {
  uint32_t u = 0;
  for (size_t i = 0; i < n; ++i) u += steps[i].units;
  return u;
}

void morse_player_init(morse_player *p, const morse_step *steps, size_t n, uint32_t ticks_per_unit, int loop) {
  p->steps = steps;
  p->n = n;
  p->i = 0;
  p->ticks_per_unit = ticks_per_unit ? ticks_per_unit : 1;
  p->loop = loop;
  p->done = (n == 0);
  p->left = n ? steps[0].units * p->ticks_per_unit : 0;
}

int morse_player_tick(morse_player *p) {
  if (p->done) return 0;
  int level = p->steps[p->i].on;
  if (--p->left == 0) {
    if (++p->i == p->n) {
      if (!p->loop) {
        p->done = 1;
        return level;
      }
      p->i = 0;
    }
    p->left = p->steps[p->i].units * p->ticks_per_unit;
  }
  return level;
}
