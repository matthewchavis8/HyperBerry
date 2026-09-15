// @file test_uart.cpp
// @brief Integration tests for PL011 UART hardware.
//
// Exercises basic UART TX on real hardware or QEMU. The RX loopback
// test is stubbed until external wiring is available.

#include "tests/integration/suite.h"
#include "uart.h"
#include "lib/log/log.h"

// @brief Verify that UART transmission can emit at least one byte without stalling.
// @return Always true if control reaches the end of the function.
static bool test_tx_doesnt_hang() {
    Uart::GetInstance().Putc('A');
    return true;
}

// @brief Verify that UART transmission can emit a string
// @return Always true if control reaches the end of the function.
static bool test_tx_string_doesnt_hang() {
    Log::Print("I have no mouth and I must scream");
    return true;
}

// @brief Verify hex formatting emits output without stalling.
//
// Uses the well-known value 0xDEADBEEFCAFEBABE so the output is visually
// identifiable in a log trace.
// @return Always true if control reaches the end of the function.
static bool test_print_hex_doesnt_hang() {
    Log::Println("{:x}", 0xDEADBEEFCAFEBABEULL);
    return true;
}

// @brief Verify hex formatting handles the all-zeros edge case without stalling.
// @return Always true if control reaches the end of the function.
static bool test_print_hex_zero_doesnt_hang() {
    Log::Println("{:x}", 0x0ULL);
    return true;
}

// @brief Verify hex formatting handles the all-ones (UINT64_MAX) edge case without stalling.
// @return Always true if control reaches the end of the function.
static bool test_print_hex_max_doesnt_hang() {
    Log::Println("{:x}", 0xFFFFFFFFFFFFFFFFULL);
    return true;
}

// @brief Verify brace-style formatting emits output without stalling.
// @return Always true if control reaches the end of the function.
static bool test_formatted_print_doesnt_hang() {
    Log::Println("value={} ok={} ptr={}", -42, true, reinterpret_cast<void*>(0x1234ULL));
    return true;
}

// Static case table for the UART hardware integration suite.
static const TestCase uart_hw_cases[] {
    { "test_tx_doesnt_hang\n", test_tx_doesnt_hang },
    { "test_tx_string_doesnt_hang\n", test_tx_string_doesnt_hang },
    { "test_print_hex_doesnt_hang\n", test_print_hex_doesnt_hang },
    { "test_print_hex_zero_doesnt_hang\n", test_print_hex_zero_doesnt_hang },
    { "test_print_hex_max_doesnt_hang\n", test_print_hex_max_doesnt_hang },
    { "test_formatted_print_doesnt_hang\n", test_formatted_print_doesnt_hang },
};

// UART integration test suite auto-registered into `.hyperberry_tests`.
static const TestSuite uartSuite {
    "UartHarness",
    uart_hw_cases,
    6,
};

REGISTER_SUITE(uartSuite);
