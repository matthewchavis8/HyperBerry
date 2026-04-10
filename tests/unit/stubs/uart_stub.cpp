/**
 * @file uart_stub.cpp
 * @brief No-op stubs for Uart methods used in unit tests.
 *
 * Satisfies link dependencies from buddy.cpp and dtb.cpp without
 * requiring real UART hardware or MMIO addresses.
 */

#include "drivers/uart/uart.h"

volatile uint32_t* Uart::reg(UART_REG) { return nullptr; }
void Uart::init() {}
void Uart::println(const char*) {}
void Uart::print(const char*) {}
void Uart::putc(const char) {}
void Uart::writeHex(uint64_t) {}
