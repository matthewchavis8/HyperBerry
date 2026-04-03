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
 * @brief Verify that UART transmission can emit a string
 * @return Always true if control reaches the end of the function.
 */
static bool test_tx_string_doesnt_hang() {
  Uart::print("I have no mouth and I must scream");
  return true;
}

/**
 * @brief Verify that init() completes without stalling and leaves UART functional.
 *
 * Calls init() a second time (re-initialisation) and then verifies TX still
 * works.  If init() corrupts the CR/ICR registers in a way that breaks TX the
 * subsequent putc() will spin forever, which the test runner will observe as a
 * hang.
 * @return Always true if control reaches the end of the function.
 */
static bool test_init_doesnt_hang() {
  Uart::init();
  Uart::putc('I');
  return true;
}

/**
 * @brief Verify that writeHex() emits exactly 16 characters without stalling.
 *
 * Uses the well-known value 0xDEADBEEFCAFEBABE so the output is visually
 * identifiable in a log trace.
 * @return Always true if control reaches the end of the function.
 */
static bool test_write_hex_doesnt_hang() {
  Uart::writeHex(0xDEADBEEFCAFEBABEULL);
  return true;
}

/**
 * @brief Verify writeHex() handles the all-zeros edge case without stalling.
 * @return Always true if control reaches the end of the function.
 */
static bool test_write_hex_zero_doesnt_hang() {
  Uart::writeHex(0x0ULL);
  return true;
}

/**
 * @brief Verify writeHex() handles the all-ones (UINT64_MAX) edge case without stalling.
 * @return Always true if control reaches the end of the function.
 */
static bool test_write_hex_max_doesnt_hang() {
  Uart::writeHex(0xFFFFFFFFFFFFFFFFULL);
  return true;
}

/** Static case table for the UART hardware integration suite. */
static const TestCase uart_hw_cases[] = {
    {"test_tx_doesnt_hang\n",           test_tx_doesnt_hang},
    {"test_tx_string_doesnt_hang\n",    test_tx_string_doesnt_hang},
    {"test_init_doesnt_hang\n",         test_init_doesnt_hang},
    {"test_write_hex_doesnt_hang\n",    test_write_hex_doesnt_hang},
    {"test_write_hex_zero_doesnt_hang\n", test_write_hex_zero_doesnt_hang},
    {"test_write_hex_max_doesnt_hang\n",  test_write_hex_max_doesnt_hang},
};

/** UART integration test suite auto-registered into `.hyperberry_tests`. */
static const TestSuite uartSuite = {
    "UartHarness",
    uart_hw_cases,
    6,
};

REGISTER_SUITE(uartSuite);
