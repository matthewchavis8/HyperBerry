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
 * @brief Print a suite header banner with the suite name and case count.
 * @param name  Suite name.
 * @param count Number of test cases in the suite.
 */
inline void suite_header(const char* name, int count) {
  Uart::println("");
  Uart::println("======== {} ({} tests) ========", name, count);
}

/**
 * @brief Emit a PASS line: `[n/total] PASS: suite: desc`.
 * @param n     Current test number (1-based).
 * @param total Total cases in the suite.
 * @param suite Suite name.
 * @param desc  Test case description.
 */
inline void ok(int n, int total, const char* suite, const char* desc) {
  Uart::println("[{}/{}] PASS: {}: {}", n, total, suite, desc);
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
  Uart::println("[{}/{}] FAIL: {}::{}", n, total, suite, desc);
  Uart::println("         reason: {}", reason);
}

/**
 * @brief Print a final summary: pass/fail counts and overall verdict.
 * @param passed Number of passed cases.
 * @param failed Number of failed cases.
 * @param total  Total cases run.
 */
inline void summary(int passed, int failed, int total) {
  Uart::println("-------- Results --------");
  Uart::println("{} passed, {} failed, {} total", passed, failed, total);
  failed == 0 ? Uart::println("TESTS PASSED") : Uart::println("TESTS FAILED");
}

} // namespace Tap

#endif // !__TAP_H__
