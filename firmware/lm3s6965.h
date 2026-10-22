/* SPDX-License-Identifier: Apache-2.0
 * The few LM3S6965 / Cortex-M3 registers this firmware touches. */
#ifndef LM3S6965_H
#define LM3S6965_H

#include <stdint.h>

#define REG(addr) (*(volatile uint32_t *)(addr))

/* System control: run-mode clock gating for GPIO ports. */
#define SYSCTL_RCGC2 REG(0x400FE108u)
#define RCGC2_GPIOF (1u << 5)

/* GPIO port F (the EK-LM3S6965 user LED is PF0). DATA is address-masked:
 * offset (mask << 2) reads/writes only the bits in mask. */
#define GPIOF_BASE 0x40025000u
#define GPIOF_DATA(mask) REG(GPIOF_BASE + ((uint32_t)(mask) << 2))
#define GPIOF_DIR REG(GPIOF_BASE + 0x400u)
#define GPIOF_DEN REG(GPIOF_BASE + 0x51Cu)
#define LED_PIN (1u << 0)

/* UART0 (PL011). */
#define UART0_BASE 0x4000C000u
#define UART0_DR REG(UART0_BASE + 0x000u)
#define UART0_FR REG(UART0_BASE + 0x018u)
#define UART0_CTL REG(UART0_BASE + 0x030u)
#define UART_FR_TXFF (1u << 5)

/* SysTick. */
#define SYST_CSR REG(0xE000E010u)
#define SYST_RVR REG(0xE000E014u)
#define SYST_CVR REG(0xE000E018u)
#define SYST_CSR_ENABLE (1u << 0)
#define SYST_CSR_TICKINT (1u << 1)
#define SYST_CSR_CLKSOURCE (1u << 2)

/* Fault status. */
#define SCB_CFSR REG(0xE000ED28u)
#define SCB_HFSR REG(0xE000ED2Cu)

#endif
