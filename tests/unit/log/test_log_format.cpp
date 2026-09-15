// @file test_log_format.cpp
// @brief Unit tests for brace-style log formatting.

#include <gtest/gtest.h>

#include "lib/log/log.h"

namespace uart_test_support {
void Reset();
const char* Buffer();
void Append(char ch);
} // namespace uart_test_support

namespace {

template <typename... Args>
void captureFormat(const char* fmt, Args... args) {
    uart_test_support::Reset();
    log::detail::FormatToSink([](char ch) { uart_test_support::Append(ch); }, fmt, args...);
}

enum class TestEnum : uint8_t {
    VALUE = 7,
};

} // namespace

TEST(Log, FormatsMixedValues) {
    uart_test_support::Reset();

    Log::Print("str={} c={} i={} u={} b={} p={}",
            "hi",
            'A',
            -42,
            7U,
            true,
            reinterpret_cast<void*>(0x1234ULL));

    EXPECT_STREQ(uart_test_support::Buffer(), "str=hi c=A i=-42 u=7 b=true p=0x1234");
}

TEST(Log, EscapesBraces) {
    captureFormat("{{{}}}", 42);
    EXPECT_STREQ(uart_test_support::Buffer(), "{42}");
}

TEST(Log, FormatLineEndsWithCrlf) {
    uart_test_support::Reset();
    log::detail::FormatLineToSink([](char ch) { uart_test_support::Append(ch); }, "x={:x}", 0x2AU);
    EXPECT_STREQ(uart_test_support::Buffer(), "x=0x2A\r\n");
}

TEST(Log, PrintlnPrintsPlainStringAsIs) {
    uart_test_support::Reset();
    Log::Println("{not a placeholder}");
    EXPECT_STREQ(uart_test_support::Buffer(), "{not a placeholder}\r\n");
}

TEST(Log, PrintsNullCString) {
    uart_test_support::Reset();

    const char* value { nullptr };
    Log::Print("{}", value);

    EXPECT_STREQ(uart_test_support::Buffer(), "(null)");
}

TEST(Log, PrintsNullptrAsPointer) {
    uart_test_support::Reset();

    Log::Print("{}", nullptr);

    EXPECT_STREQ(uart_test_support::Buffer(), "0x0");
}

TEST(Log, PrintsEnumsAsIntegers) {
    uart_test_support::Reset();

    Log::Print("{}", TestEnum::VALUE);

    EXPECT_STREQ(uart_test_support::Buffer(), "7");
}

TEST(Log, ReportsMissingArgument) {
    captureFormat("x={} y={}", 1);
    EXPECT_STREQ(uart_test_support::Buffer(), "x=1 y=[missing arg]");
}

TEST(Log, ReportsExtraArgument) {
    captureFormat("x={}", 1, 2);
    EXPECT_STREQ(uart_test_support::Buffer(), "x=1[extra arg]");
}

TEST(Log, ReportsInvalidFormat) {
    captureFormat("x={foo}", 1);
    EXPECT_STREQ(uart_test_support::Buffer(), "x=[invalid format]");
}

TEST(Log, FormatsHexIntegers) {
    captureFormat("value={:x} enum={:x} bool={:x}", 0x2AU, TestEnum::VALUE, true);
    EXPECT_STREQ(uart_test_support::Buffer(), "value=0x2A enum=0x7 bool=0x1");
}

TEST(Log, FormatsHexPointers) {
    captureFormat("ptr={:x}", reinterpret_cast<void*>(0x1234ULL));
    EXPECT_STREQ(uart_test_support::Buffer(), "ptr=0x1234");
}

TEST(Log, PrintlnAppendsCrlf) {
    uart_test_support::Reset();

    Log::Println("{} {}", 1, 2);

    EXPECT_STREQ(uart_test_support::Buffer(), "1 2\r\n");
}
