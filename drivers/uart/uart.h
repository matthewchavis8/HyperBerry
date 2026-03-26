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
 * @ingroup drivers_uart
 * @brief Static PL011 UART driver for early debug output.
 *
 * All methods are static — this class is never instantiated.
 * It provides the minimum serial output needed before a full
 * memory allocator and exception model are in place.
 */
class Uart {
  private:
    // PL011 UART base address, selected per target board.
    #ifdef QEMU
    static constexpr uint64_t UART_BASE = 0x09000000;
    #else
    static constexpr uint64_t UART_BASE = 0x7E201000;
    #endif

    // Flag Register bit mask — TX FIFO full (bit 5).
    static constexpr uint32_t FR_TXFF = (1 << 5);

    /**
     * @brief Transmit a single character via the UART Data Register.
     * @param ch Character to send.
     * @warning Busy-waits until the TX FIFO has space. Do not call
     *          from an interrupt context or time-critical path.
     */
    static void putc(const char ch);

  public:
    /**
     * @brief Transmit a null-terminated string.
     * @param str Pointer to the null-terminated string to send.
     * @note Each character is sent via putc(), which spins on the
     *       TX FIFO. For large strings this blocks proportionally.
     */
    static void print(const char* str);
};

#endif // !__UART_H__
