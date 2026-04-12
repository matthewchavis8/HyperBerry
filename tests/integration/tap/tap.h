/**
 * @file tap.h
 * @brief Freestanding TAP-style test output emitter over UART.
 *
 * Header-only, no stdlib. All output goes through Uart::println() and
 * Uart::print(). Provides suite headers, per-case PASS/FAIL lines with
 * progress counters, and a final summary.
 *
 * @note Uses Uart::println() for all line endings to emit proper CRLF
 *       sequences required by raw UART hardware (e.g. Raspberry Pi 5).
 *       Never embed bare '\n' in print() calls.
 */

#ifndef __TAP_H__
#define __TAP_H__

#include "uart.h"

namespace Tap {

/**
 * @brief Print a positive integer as decimal digits over UART.
 * @param n Non-negative integer to print.
 */
inline void print_int(int n) {
  if (n == 0) {
    Uart::putc('0');
    return;
  }

  char buf[10];
  int len = 0;

  while (n > 0) {
    buf[len++] = '0' + (n % 10);
    n /= 10;
  }

  for (int i = len - 1; i >= 0; i--) {
    Uart::putc(buf[i]);
  }
}

/**
 * @brief Print a suite header banner with the suite name and case count.
 * @param name  Suite name.
 * @param count Number of test cases in the suite.
 */
inline void suite_header(const char* name, int count) {
  Uart::println("");
  Uart::print("======== ");
  Uart::print(name);
  Uart::print(" (");
  print_int(count);
  Uart::println(" tests) ========");
}

/**
 * @brief Emit a PASS line: `[n/total] PASS: suite: desc`.
 * @param n     Current test number (1-based).
 * @param total Total cases in the suite.
 * @param suite Suite name.
 * @param desc  Test case description.
 */
inline void ok(int n, int total, const char* suite, const char* desc) {
  Uart::print("[");
  print_int(n);
  Uart::print("/");
  print_int(total);
  Uart::print("] PASS: ");
  Uart::print(suite);
  Uart::print(": ");
  Uart::println(desc);
}

/**
 * @brief Emit a FAIL line with reason: `[n/total] FAIL: suite::desc`.
 * @param n      Current test number (1-based).
 * @param total  Total cases in the suite.
 * @param suite  Suite name.
 * @param desc   Test case description.
 * @param reason Human-readable failure reason.
 */
inline void fail(int n, int total, const char* suite, const char* desc,
                 const char* reason) {
  Uart::print("[");
  print_int(n);
  Uart::print("/");
  print_int(total);
  Uart::print("] FAIL: ");
  Uart::print(suite);
  Uart::print("::");
  Uart::println(desc);
  Uart::print("         reason: ");
  Uart::println(reason);
}

/**
 * @brief Print a final summary: pass/fail counts and overall verdict.
 * @param passed Number of passed cases.
 * @param failed Number of failed cases.
 * @param total  Total cases run.
 */
inline void summary(int passed, int failed, int total) {
  Uart::println("-------- Results --------");
  print_int(passed);
  Uart::print(" passed, ");
  print_int(failed);
  Uart::print(" failed, ");
  print_int(total);
  Uart::println(" total");
  failed == 0 ? Uart::println("TESTS PASSED") : Uart::println("TESTS FAILED");
}

} // namespace Tap

#endif // !__TAP_H__
