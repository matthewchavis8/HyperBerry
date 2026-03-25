#ifndef __UART_H__
#define __UART_H__

#include <stdint.h>

class Uart {
  private:
    // PL011 UART BASE Address (QEMU virt machine)
    #ifdef QEMU
    static constexpr uint64_t UART_BASE = 0x09000000; // QEMU Base
    #else
    static constexpr uint64_t UART_BASE = 0x7E201000; // RPI5 Base
    #endif

    // TX FIFO full
    static constexpr uint32_t FR_TXFF = (1 << 5);

    // @fn: Writes a single character into the Data register to trasmit
    static void putc(const char c);

  public:
    static void print(const char* str);
};

#endif // !__UART_H__
