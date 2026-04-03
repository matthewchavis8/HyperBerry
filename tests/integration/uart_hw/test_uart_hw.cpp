/**
 * @file test_uart_hw.cpp
 * @brief Integration tests for PL011 UART hardware.
 *
 * Exercises basic UART TX on real hardware or QEMU. The RX loopback
 * test is stubbed until external wiring is available.
 */

#include "tests/integration/suite.h"
#include "uart.h"

/**
 * @brief Verify that UART transmission can emit at least one byte without stalling.
 * @return Always true if control reaches the end of the function.
 */
static bool test_tx_doesnt_hang() {
  Uart::putc('A');
  return true;
}

/**
 * @brief Placeholder for a future RX loopback test that requires external wiring.
 * @return Always true until physical loopback coverage is implemented.
 */
static bool test_rx_loopback_stub() {
  // RX loopback requires hardware wiring — stub passes for now
  return true;
}

/** Static case table for the UART hardware integration suite. */
static const TestCase uart_hw_cases[] = {
    {"tx doesnt hang", test_tx_doesnt_hang},
    {"rx loopback stub", test_rx_loopback_stub},
};

/** UART integration test suite auto-registered into `.hyperberry_tests`. */
static const TestSuite uart_hw_suite = {
    "uart_hw",
    uart_hw_cases,
    2,
};

REGISTER_SUITE(uart_hw_suite);
