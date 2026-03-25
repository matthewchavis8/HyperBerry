/**
 * @file uart.h
 * @brief PL011 UART driver for serial output.
 *
 * Provides basic transmit functionality over the PL011 UART peripheral
 * for both QEMU virt and Raspberry Pi 5 hardware targets. The base
 * address is selected at compile time via the QEMU preprocessor define.
 */

#ifndef __UART_H__
#define __UART_H__

#include <stdint.h>

/**
 * @class Uart
 * @brief Static PL011 UART driver for early debug output.
 */
class Uart {
  private:
    /** @brief PL011 UART base address, selected per target board. */
    #ifdef QEMU
    static constexpr uint64_t UART_BASE = 0x09000000;
    #else
    static constexpr uint64_t UART_BASE = 0x7E201000;
    #endif

    /** @brief Flag Register bit mask — TX FIFO full (bit 5). */
    static constexpr uint32_t FR_TXFF = (1 << 5);

    /**
     * @brief Transmit a single character via the UART Data Register.
     * @param c Character to send.
     */
    static void putc(const char c);

  public:
    /**
     * @brief Transmit a null-terminated string.
     * @param str Pointer to the string to send.
     */
    static void print(const char* str);
};

#endif // !__UART_H__
