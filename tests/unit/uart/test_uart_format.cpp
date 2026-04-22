/**
 * @file test_uart_format.cpp
 * @brief Unit tests for brace-style UART formatting.
 */

#include <gtest/gtest.h>

#include "drivers/uart/uart.h"

namespace uart_test_support {
void reset();
const char* buffer();
void append(char ch);
} // namespace uart_test_support

namespace {

template<typename... Args>
void captureFormat(const char* fmt, Args... args) {
  uart_test_support::reset();
  uart::detail::formatToSink([](char ch) { uart_test_support::append(ch); }, fmt, args...);
}

enum class TestEnum : uint8_t {
  Value = 7,
};

} // namespace

TEST(Uart, FormatsMixedValues) {
  uart_test_support::reset();

  Uart::print("str={} c={} i={} u={} b={} p={}",
              "hi",
              'A',
              -42,
              7U,
              true,
              reinterpret_cast<void*>(0x1234ULL));

  EXPECT_STREQ(uart_test_support::buffer(),
               "str=hi c=A i=-42 u=7 b=true p=0x1234");
}

TEST(Uart, EscapesBraces) {
  captureFormat("{{{}}}", 42);
  EXPECT_STREQ(uart_test_support::buffer(), "{42}");
}

TEST(Uart, PrintsNullCString) {
  uart_test_support::reset();

  const char* value = nullptr;
  Uart::print("{}", value);

  EXPECT_STREQ(uart_test_support::buffer(), "(null)");
}

TEST(Uart, PrintsNullptrAsPointer) {
  uart_test_support::reset();

  Uart::print("{}", nullptr);

  EXPECT_STREQ(uart_test_support::buffer(), "0x0");
}

TEST(Uart, PrintsEnumsAsIntegers) {
  uart_test_support::reset();

  Uart::print("{}", TestEnum::Value);

  EXPECT_STREQ(uart_test_support::buffer(), "7");
}

TEST(Uart, ReportsMissingArgument) {
  captureFormat("x={} y={}", 1);
  EXPECT_STREQ(uart_test_support::buffer(), "x=1 y=[missing arg]");
}

TEST(Uart, ReportsExtraArgument) {
  captureFormat("x={}", 1, 2);
  EXPECT_STREQ(uart_test_support::buffer(), "x=1[extra arg]");
}

TEST(Uart, ReportsInvalidFormat) {
  captureFormat("x={foo}", 1);
  EXPECT_STREQ(uart_test_support::buffer(), "x=[invalid format]");
}

TEST(Uart, FormatsHexIntegers) {
  captureFormat("value={:x} enum={:x} bool={:x}", 0x2AU, TestEnum::Value, true);
  EXPECT_STREQ(uart_test_support::buffer(), "value=0x2A enum=0x7 bool=0x1");
}

TEST(Uart, FormatsHexPointers) {
  captureFormat("ptr={:x}", reinterpret_cast<void*>(0x1234ULL));
  EXPECT_STREQ(uart_test_support::buffer(), "ptr=0x1234");
}

TEST(Uart, PrintlnAppendsCrlf) {
  uart_test_support::reset();

  Uart::println("{} {}", 1, 2);

  EXPECT_STREQ(uart_test_support::buffer(), "1 2\r\n");
}
