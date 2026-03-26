/**
 * @file uart.cpp
 * @brief PL011 UART driver implementation.
 * @ingroup drivers_uart
 */

#include "uart.h"
#include <stdint.h>

/**
 * @brief Transmit a single character via the UART Data Register.
 *
 * Polls the Flag Register (FR, offset 0x18) until bit 5 (TXFF) clears,
 * then writes the character to the Data Register (DR, offset 0x00).
 *
 * @note Busy-waits on FR_TXFF. This is safe during early boot when
 *       no scheduler or interrupt controller is active yet.
 */
void Uart::putc(const char ch) {
  volatile uint32_t* DR = reinterpret_cast<volatile uint32_t*>(UART_BASE + 0x00);
  volatile uint32_t* FR = reinterpret_cast<volatile uint32_t*>(UART_BASE + 0x18);

  // Spin while TX is full
  while ((*FR & FR_TXFF) != 0U) {}

  *DR = static_cast<uint32_t>(static_cast<uint8_t>(ch));
}

/** @brief Transmit a null-terminated string character by character. */
void Uart::print(const char* str) {
  while (*str != '\0') {
    putc(*str);
    ++str;
  }
}
