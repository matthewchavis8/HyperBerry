#include "uart.h"
#include <stdint.h>

void Uart::putc(const char ch) {
  volatile uint32_t* DR = reinterpret_cast<volatile uint32_t*>(UART_BASE + 0x00);
  volatile uint32_t* FR = reinterpret_cast<volatile uint32_t*>(UART_BASE + 0x18);

  // Spin while TX is full
  while ((*FR & FR_TXFF) != 0U) {}

  *DR = static_cast<uint32_t>(static_cast<uint8_t>(ch));
}

void Uart::print(const char* str) {
  while (*str != '\0') {
    putc(*str);
    ++str;
  }
}
