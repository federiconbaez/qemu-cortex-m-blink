/* SPDX-License-Identifier: Apache-2.0 */
#ifndef BOARD_H
#define BOARD_H

#include <stdint.h>

void uart_init(void);
void uart_puts(const char *s);
void uart_putu(uint32_t v);
void uart_puthex(uint32_t v);

/* ARM semihosting SYS_EXIT_EXTENDED: QEMU exits with `code`. */
__attribute__((noreturn)) void semihost_exit(uint32_t code);

#endif
