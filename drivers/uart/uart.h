/**
 * @file uart.h
 * @brief PL011 UART driver for serial I/O.
 *
 * Provides transmit and receive functionality over the PL011 UART peripheral
 * for both QEMU virt and Raspberry Pi 5 hardware targets. The MMIO base
 * address is selected from the active platform definition.
 */

#ifndef __UART_H__
#define __UART_H__

#include "platform/platform_def.h"
#include <stdint.h>

/**
 * @brief Uppercase hexadecimal digit lookup table used by writeHex().
 * @ingroup drivers_uart
 */
static constexpr char hex[] = "0123456789ABCDEF";

/**
 * @brief PL011 register offsets from @ref Platform::kUartBase.
 * @ingroup drivers_uart
 */
enum class UART_REG : uint8_t {
    DR   = 0x00,
    FR   = 0x18,
    IBRD = 0x24,
    FBRD = 0x28,
    LCRH = 0x2C,
    CR   = 0x30,
    ICR  = 0x44,
};

/**
 * @class Uart
 * @ingroup drivers_uart
 * @brief Static PL011 UART driver for early serial I/O.
 *
 */
class Uart {
  private:
    /**
     * @brief returns the register address for UART PL011
     *
     * @param reg PL011 register offset.
     * @return Pointer to the memory-mapped PL011 register.
     *
     * */
    static volatile uint32_t* reg(UART_REG reg);

  public:
    /**
     * @brief Initialize the PL011 UART.
     *
     * @note Must be called before print().
     */
    static void init();

    /**
     * @brief Transmit a null-terminated string.
     * @param str Pointer to the null-terminated string to send.
     * @note Each character is sent via putc(), which spins on the
     *       TX FIFO. For large strings this blocks proportionally.
     */
    static void println(const char* str);
    
    /**
     * @brief Transmit a null-terminated string.
     * @param str Pointer to the null-terminated string to send.
     * @note Each character is sent via putc(), which spins on the
     *       TX FIFO. For large strings this blocks proportionally.
     */
    static void print(const char* str);

    /**
     * @brief Transmit a single character over the UART.
     * @param ch Character to send.
     * @warning Busy-waits until the TX FIFO has space. Do not call
     *          from an interrupt context or time-critical path.
     */
    static void putc(const char ch);

    /**
     * @brief Write a 64-bit value as a 16-digit hexadecimal string.
     * @param val  The value to print.
     * @note Always emits exactly 16 hex digits (zero-padded).
     *       Does not print a "0x" prefix -- callers must add it
     *       themselves.
     */
    static void writeHex(uint64_t val);
};

#endif // !__UART_H__
