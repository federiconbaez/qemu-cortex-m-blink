/* SPDX-License-Identifier: Apache-2.0
 * Blinks "SOS " in Morse on PF0 from the SysTick interrupt, records every
 * edge as read back from the GPIO data register, runs on-target checks and
 * exits QEMU through semihosting with the number of failures. */
#include <stdint.h>

#include "board.h"
#include "lm3s6965.h"
#include "morse.h"

#define CPU_HZ 12000000u /* nominal; the checks count ticks and never depend on wall time */
#define TICK_HZ 1000u
#define TICKS_PER_UNIT 2u
#define LOOPS 2u
#define MAX_EDGES 64u

uint32_t stack_high_water(void);
uint32_t stack_size(void);

static volatile uint32_t data_sentinel = 0x5EED1234u;
static volatile uint32_t bss_sentinel;

static morse_step steps[64];
static morse_player player;
static volatile uint32_t ticks;
static volatile uint32_t blink_ticks_left;
static volatile uint32_t blink_tick;
static volatile uint32_t n_edges;
static volatile uint32_t edge_tick[MAX_EDGES];
static volatile uint8_t edge_level[MAX_EDGES];
static uint32_t last_hw;

static int failures;

static void check(int ok, const char *what) {
  uart_puts(ok ? "ok   " : "FAIL ");
  uart_puts(what);
  uart_puts("\n");
  if (!ok) ++failures;
}

void SysTick_Handler(void) {
  if (blink_ticks_left) {
    int level = morse_player_tick(&player);
    GPIOF_DATA(LED_PIN) = level ? LED_PIN : 0u;
    /* Edges are taken from the pin as the GPIO block reports it, not from
     * the player's own state. */
    uint32_t hw = (GPIOF_DATA(LED_PIN) & LED_PIN) ? 1u : 0u;
    if (hw != last_hw && n_edges < MAX_EDGES) {
      edge_tick[n_edges] = blink_tick;
      edge_level[n_edges] = (uint8_t)hw;
      ++n_edges;
    }
    last_hw = hw;
    ++blink_tick;
    --blink_ticks_left;
  }
  ++ticks;
}

static void test_startup(void) {
  check(data_sentinel == 0x5EED1234u, ".data copied from flash");
  check(bss_sentinel == 0u, ".bss zeroed");
}

static void test_gpio(void) {
  SYSCTL_RCGC2 |= RCGC2_GPIOF;
  GPIOF_DIR = 0x03u; /* PF0, PF1 outputs; PF2.. inputs */
  GPIOF_DEN = 0x03u;
  GPIOF_DATA(0xFFu) = 0u;
  GPIOF_DATA(0x01u) = 0xFFu;
  check(GPIOF_DATA(0xFFu) == 0x01u, "masked write sets only PF0");
  GPIOF_DATA(0x02u) = 0xFFu;
  GPIOF_DATA(0x01u) = 0x00u;
  check(GPIOF_DATA(0xFFu) == 0x02u, "masked write clears PF0, keeps PF1");
  check(GPIOF_DATA(0x01u) == 0u, "masked read hides unmasked bits");
  GPIOF_DATA(0x04u) = 0xFFu;
  check((GPIOF_DATA(0xFFu) & 0x04u) == 0u, "input pin PF2 ignores writes");
  GPIOF_DATA(0xFFu) = 0u;
}

static void test_systick(void) {
  SYST_RVR = CPU_HZ / TICK_HZ - 1u;
  SYST_CVR = 0u;
  SYST_CSR = SYST_CSR_ENABLE | SYST_CSR_TICKINT | SYST_CSR_CLKSOURCE;
  uint32_t start = ticks;
  uint32_t wakeups = 0;
  while (ticks - start < 10u) {
    __asm__ volatile("wfi");
    ++wakeups;
  }
  check(ticks - start >= 10u, "SysTick interrupt advances the tick count");
  check(wakeups >= 10u && wakeups <= 11u, "wfi sleeps until each tick (one wakeup per tick)");
  check(SYST_CVR <= SYST_RVR, "SysTick current value within reload");
}

static void test_blink(void) {
  int n = morse_compile("SOS ", steps, 64, 0);
  check(n == 18 && morse_units(steps, (uint32_t)n) == 34u, "SOS compiles to 18 steps, 34 units");
  morse_player_init(&player, steps, (uint32_t)n, TICKS_PER_UNIT, 1);
  const uint32_t total = 34u * TICKS_PER_UNIT * LOOPS;
  __asm__ volatile("cpsid i");
  last_hw = 0;
  blink_ticks_left = total;
  __asm__ volatile("cpsie i");
  while (blink_ticks_left) __asm__ volatile("wfi");
  check(blink_tick == total, "blink ran for exactly 2 loops of ticks");
  check(n_edges == 36u, "36 edges on PF0 (9 pulses per loop)");
  /* Replay the same sequence in software and compare tick by tick. */
  morse_player ref;
  morse_player_init(&ref, steps, (uint32_t)n, TICKS_PER_UNIT, 1);
  uint32_t k = 0, prev = 0;
  int same = 1;
  for (uint32_t t = 0; t < total; ++t) {
    uint32_t lv = (uint32_t)morse_player_tick(&ref);
    if (lv != prev) {
      same = same && k < n_edges && edge_tick[k] == t && edge_level[k] == lv;
      ++k;
    }
    prev = lv;
  }
  check(same && k == n_edges, "hardware edges match the software replay");
  for (uint32_t i = 0; i < n_edges; ++i) {
    uart_puts("edge ");
    uart_putu(edge_tick[i]);
    uart_puts(" ");
    uart_putu(edge_level[i]);
    uart_puts("\n");
  }
  check(GPIOF_DATA(LED_PIN) == 0u, "LED off at the end (word gap)");
}

int main(void) {
  uart_init();
  uart_puts("qemu-cortex-m-blink\n");
#ifdef FAULT_TEST
  uart_puts("executing an undefined instruction\n");
  __asm__ volatile("udf #0");
#endif
  test_startup();
  test_gpio();
  test_systick();
  test_blink();
  SYST_CSR = 0u;
  uint32_t hw = stack_high_water();
  uart_puts("stack high water ");
  uart_putu(hw);
  uart_puts(" of ");
  uart_putu(stack_size());
  uart_puts(" bytes\n");
  check(hw < stack_size() / 2u, "stack use below half the reserved stack");
  uart_puts(failures ? "FAIL\n" : "PASS\n");
  return failures;
}
