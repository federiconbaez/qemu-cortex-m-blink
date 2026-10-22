/* SPDX-License-Identifier: Apache-2.0
 * UART0 output and semihosting exit. */
#include "board.h"

#include "lm3s6965.h"

void uart_init(void) {
  UART0_CTL = 0x301u; /* UARTEN | TXE | RXE */
}

static void uart_putc(char c) {
  while (UART0_FR & UART_FR_TXFF) {
  }
  UART0_DR = (uint32_t)(unsigned char)c;
}

void uart_puts(const char *s) {
  while (*s) uart_putc(*s++);
}

void uart_putu(uint32_t v) {
  char buf[11];
  int i = 10;
  buf[i] = '\0';
  do {
    buf[--i] = (char)('0' + v % 10u);
    v /= 10u;
  } while (v);
  uart_puts(&buf[i]);
}

void uart_puthex(uint32_t v) {
  static const char hexd[] = "0123456789abcdef";
  char buf[11];
  buf[0] = '0';
  buf[1] = 'x';
  for (int i = 0; i < 8; ++i) buf[2 + i] = hexd[(v >> (28 - 4 * i)) & 0xFu];
  buf[10] = '\0';
  uart_puts(buf);
}

#define SYS_EXIT_EXTENDED 0x20u
#define ADP_STOPPED_APPLICATION_EXIT 0x20026u

void semihost_exit(uint32_t code) {
  volatile uint32_t block[2] = {ADP_STOPPED_APPLICATION_EXIT, code};
  register uint32_t r0 __asm__("r0") = SYS_EXIT_EXTENDED;
  register volatile uint32_t *r1 __asm__("r1") = block;
  __asm__ volatile("bkpt 0xAB" : "+r"(r0) : "r"(r1) : "memory");
  for (;;) {
  }
}
