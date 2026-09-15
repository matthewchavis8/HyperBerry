// @file uart.cpp
// @brief PL011 UART driver implementation.
// @ingroup drivers_uart

#include "uart.h"
#include "regs.inc"
#include "lib/mmio/mmio.h"
#include <stdint.h>

// Starts at the compile-time BSP value so the early console works before the
// device tree has been parsed; discovery may repoint it afterwards.
Uart::Uart() : m_base { BSP_UART_BASE } {
    configure();

#ifndef NDEBUG
    // Written through `this`, deliberately not through Log. Log's sink calls
    // GetInstance(), and the guard byte for that static is only set once this
    // constructor returns, so routing this line through Log recurses into
    // GetInstance() until the stack is gone.
    for (const char* msg = "[UART] UART intialized\r\n"; *msg != '\0'; ++msg) {
        Putc(*msg);
    }
#endif
}

Uart& Uart::GetInstance() {
    static Uart console;
    return console;
}

void Uart::SetBase(uint64_t base) {
    if (base == m_base) return;

    m_base = base;
    configure();
}

uint64_t Uart::GetBase() const {
    return m_base;
}

void Uart::configure() const {
    // clear all stale interrupts
    mmio::Write<uint32_t>(m_base, UART_REG::ICR, 0x7FF);

    // Enable UART, TXE, RXE
    mmio::Write<uint32_t>(m_base, UART_REG::CR, (1 << 0) | (1 << 8) | (1 << 9));
}

void Uart::Putc(const char ch) const {
    // Flag Register bit mask
    constexpr uint32_t FR_TXFF = (1 << 5);

    // Spin while TX FIFO is full
    while ((mmio::Read<uint32_t>(m_base, UART_REG::FR) & FR_TXFF) != 0U) {
    }

    mmio::Write<uint32_t>(m_base, UART_REG::DR, static_cast<uint32_t>(static_cast<uint8_t>(ch)));
}
