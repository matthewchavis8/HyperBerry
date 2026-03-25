/**
 * @file uart.cpp
 * @brief PL011 UART driver implementation.
 */

#include "uart.h"
#include <stdint.h>

/**
 * @brief Transmit a single character via the UART Data Register.
 *
 * Spins on the Flag Register until the TX FIFO has space,
 * then writes the character to the Data Register.
 *
 * @param ch Character to send.
 */
void Uart::putc(const char ch) {
  volatile uint32_t* DR = reinterpret_cast<volatile uint32_t*>(UART_BASE + 0x00);
  volatile uint32_t* FR = reinterpret_cast<volatile uint32_t*>(UART_BASE + 0x18);

  // Spin while TX is full
  while ((*FR & FR_TXFF) != 0U) {}

  *DR = static_cast<uint32_t>(static_cast<uint8_t>(ch));
}

/**
 * @brief Transmit a null-terminated string character by character.
 * @param str Pointer to the string to send.
 */
void Uart::print(const char* str) {
  while (*str != '\0') {
    putc(*str);
    ++str;
  }
}
