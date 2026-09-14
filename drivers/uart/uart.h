// @file uart.h
// @brief PL011 UART driver for serial I/O.
//
// Provides transmit functionality over the PL011 UART peripheral for both
// QEMU virt and Raspberry Pi 5 hardware targets. The MMIO base address starts
// at the active BSP definition and may be repointed once the device tree has
// been read.
//
// Formatting and the console API live in @ref Log; this is the hardware.

#ifndef __UART_H__
#define __UART_H__

#include <stdint.h>

// @brief PL011 register offsets from the active base address.
// @ingroup drivers_uart
enum class UART_REG : uint8_t {
    DR = 0x00,
    FR = 0x18,
    IBRD = 0x24,
    FBRD = 0x28,
    LCRH = 0x2C,
    CR = 0x30,
    ICR = 0x44,
};

// @class Uart
// @ingroup drivers_uart
// @brief Static PL011 UART driver.
class Uart {
private:
    // @brief Program the frame at m_base: clear stale interrupts, enable TX/RX.
    // @return Nothing.
    void configure() const;

    // @brief Construct bound to the board's compile-time console base, and
    //        bring that frame up.
    //
    // Private because @ref getInstance() is the only legitimate way to reach
    // the one physical PL011. Defined out of line so the board's generated
    // `regs.inc` stays in the .cpp and off every consumer's include path.
    Uart();

    uint64_t m_base;

public:
    // @brief The board's one physical PL011.
    // @ingroup drivers_uart
    //
    // Constructed on first call rather than at static-init time.
    //
    // @return Reference to the single console driver instance.
    static Uart& getInstance();

    // @brief Rebind the driver to a base address discovered at runtime.
    // @ingroup drivers_uart
    //
    // The compile-time BSP value backs the early console, which has to work
    // before the device tree can be read. Once it has been, this repoints the
    // driver at whatever the tree actually describes.
    //
    // @param base CPU-physical base address of the PL011 register frame.
    // @return Nothing.
    void setBase(uint64_t base);

    // @brief Base address the driver is currently bound to.
    // @ingroup drivers_uart
    // @return CPU-physical base address of the active PL011 register frame.
    [[nodiscard]] uint64_t getBase() const;

    // @brief Transmit a single character over the UART.
    // @param ch Character to send.
    // @warning Busy-waits until the TX FIFO has space. Do not call
    //          from an interrupt context or time-critical path.
    // @return Nothing.
    void putc(const char ch) const;

    Uart(const Uart&) = delete;
    Uart& operator=(const Uart&) = delete;
    Uart(Uart&&) = delete;
    Uart& operator=(Uart&&) = delete;

    ~Uart() = default;
};

#endif // !__UART_H__
