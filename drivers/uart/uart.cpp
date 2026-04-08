/**
 * @file uart.cpp
 * @brief PL011 UART driver implementation.
 * @ingroup drivers_uart
 */

#include "uart.h"
#include <stdint.h>

volatile uint32_t* Uart::reg(UART_REG reg) {
  return reinterpret_cast<volatile uint32_t*>(UART_BASE + static_cast<uint64_t>(reg));
}

void Uart::init() {
  // clear all stale interrupts
  *reg(UART_REG::ICR) = 0x7FF;

  // Enable UART, TXE, RXE
  *reg(UART_REG::CR) = (1 << 0) | (1 << 8) | (1 << 9);
}

void Uart::putc(const char ch) {
  // Flag Register bit mask
  constexpr uint32_t FR_TXFF = (1 << 5);

  // Spin while TX FIFO is full
  while ((*reg(UART_REG::FR) & FR_TXFF) != 0U) {}

  *reg(UART_REG::DR) = static_cast<uint32_t>(static_cast<uint8_t>(ch));
}

void Uart::println(const char* str) {
  while (*str != '\0') {
    putc(*str);
    ++str;
  }

  putc('\n');
}

void Uart::print(const char *str) {
  while (*str != '\0') {
    putc(*str);
    ++str;
  }
}

void Uart::writeHex(uint64_t val) {
  char buff[16];

  for (int i{15}; i >= 0; i--) {
    buff[i] = hex[val & 0xF];
    val >>= 4;
  }

  for (int i{}; i < 16; i++) {
    putc(buff[i]);
  }
}
