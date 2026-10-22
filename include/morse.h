/* SPDX-License-Identifier: Apache-2.0
 * Morse code compiler and tick-driven player. Freestanding: no libc, no
 * allocation, so the same file runs on the host tests and on the Cortex-M3.
 *
 * Timing (ITU-R M.1677): dot 1 unit, dash 3, gap inside a letter 1,
 * between letters 3, between words 7. */
#ifndef MORSE_H
#define MORSE_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
  uint8_t on;     /* 1 = LED on */
  uint8_t units;  /* duration in units, 1..7 */
} morse_step;

enum { MORSE_EBADCHAR = -1, MORSE_ENOSPC = -2 };

/* Returns the ITU pattern for c ('.' and '-'), or NULL if c has none.
 * Letters are case-insensitive. */
const char *morse_pattern(char c);

/* Compiles NUL-terminated text into alternating on/off steps. Runs of
 * spaces are one word gap; leading spaces are ignored; a trailing space
 * ends the sequence with a word gap (so it loops cleanly). Returns the
 * number of steps, or MORSE_EBADCHAR (with *bad = index) / MORSE_ENOSPC. */
int morse_compile(const char *text, morse_step *out, size_t cap, size_t *bad);

/* Sum of all step durations. */
uint32_t morse_units(const morse_step *steps, size_t n);

typedef struct {
  const morse_step *steps;
  size_t n, i;
  uint32_t left;            /* ticks left in step i */
  uint32_t ticks_per_unit;
  int loop;
  int done;
} morse_player;

void morse_player_init(morse_player *p, const morse_step *steps, size_t n, uint32_t ticks_per_unit, int loop);

/* LED level for the current tick, then advances one tick. After a
 * non-looping sequence ends, returns 0 and sets done. */
int morse_player_tick(morse_player *p);

#endif
