/* SPDX-License-Identifier: Apache-2.0
 * Vector table, reset, stack painting and the HardFault reporter. */
#include <stddef.h>
#include <stdint.h>

#include "board.h"
#include "lm3s6965.h"

extern uint32_t _sidata, _sdata, _edata, _sbss, _ebss, _sstack, _estack;
int main(void);
void SysTick_Handler(void);

#define STACK_PAINT 0xA5A5A5A5u

/* The compiler may emit calls to these even with -ffreestanding. */
void *memcpy(void *dst, const void *src, size_t n) {
  unsigned char *d = dst;
  const unsigned char *s = src;
  while (n--) *d++ = *s++;
  return dst;
}

void *memset(void *dst, int c, size_t n) {
  unsigned char *d = dst;
  while (n--) *d++ = (unsigned char)c;
  return dst;
}

int memcmp(const void *a, const void *b, size_t n) {
  const unsigned char *x = a, *y = b;
  for (; n; --n, ++x, ++y)
    if (*x != *y) return *x < *y ? -1 : 1;
  return 0;
}

__attribute__((noreturn)) void Reset_Handler(void) {
  uint32_t *src = &_sidata;
  for (uint32_t *dst = &_sdata; dst < &_edata;) *dst++ = *src++;
  for (uint32_t *dst = &_sbss; dst < &_ebss;) *dst++ = 0;
  /* Paint the unused stack so main can report the high-water mark. */
  uint32_t *sp;
  __asm__ volatile("mov %0, sp" : "=r"(sp));
  for (uint32_t *p = &_sstack; p < sp - 16;) *p++ = STACK_PAINT;
  semihost_exit((uint32_t)main());
}

uint32_t stack_high_water(void) {
  const uint32_t *p = &_sstack;
  while (p < &_estack && *p == STACK_PAINT) ++p;
  return (uint32_t)((uintptr_t)&_estack - (uintptr_t)p);
}

uint32_t stack_size(void) {
  return (uint32_t)((uintptr_t)&_estack - (uintptr_t)&_sstack);
}

__attribute__((used, noreturn)) void hardfault_report(const uint32_t *frame) {
  uart_puts("hardfault pc=");
  uart_puthex(frame[6]);
  uart_puts(" cfsr=");
  uart_puthex(SCB_CFSR);
  uart_puts(" hfsr=");
  uart_puthex(SCB_HFSR);
  uart_puts("\n");
  semihost_exit(3);
}

/* Picks the stack the fault frame was pushed to, then reports it. */
__attribute__((naked)) void HardFault_Handler(void) {
  __asm__ volatile(
      "tst lr, #4\n"
      "ite eq\n"
      "mrseq r0, msp\n"
      "mrsne r0, psp\n"
      "b hardfault_report\n");
}

__attribute__((noreturn)) void Default_Handler(void) {
  uart_puts("unexpected exception\n");
  semihost_exit(4);
}

typedef void (*vector)(void);

__attribute__((section(".isr_vector"), used)) const vector vectors[16 + 8] = {
    (vector)(uintptr_t)&_estack,
    Reset_Handler,
    Default_Handler,   /* NMI */
    HardFault_Handler,
    Default_Handler,   /* MemManage */
    Default_Handler,   /* BusFault */
    Default_Handler,   /* UsageFault */
    0, 0, 0, 0,
    Default_Handler,   /* SVCall */
    Default_Handler,   /* DebugMonitor */
    0,
    Default_Handler,   /* PendSV */
    SysTick_Handler,
    Default_Handler, Default_Handler, Default_Handler, Default_Handler,
    Default_Handler, Default_Handler, Default_Handler, Default_Handler,
};
