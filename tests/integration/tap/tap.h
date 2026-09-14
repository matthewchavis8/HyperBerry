/**
 * @file tap.h
 * @brief Freestanding TAP-style test output emitter over UART.
 *
 * Header-only, no stdlib. All output goes through Log::writeLine() and
 * Log::write(). Provides suite headers, per-case PASS/FAIL lines with
 * progress counters, and a final summary.
 *
 * @note Uses Log::writeLine() for all line endings to emit proper CRLF
 *       sequences required by raw UART hardware (e.g. Raspberry Pi 5).
 *       Never embed bare '\n' in print() calls.
 */

#ifndef __TAP_H__
#define __TAP_H__

#include "lib/log/log.h"

namespace Tap {

/**
 * @brief Print a suite header banner with the suite name and case count.
 * @param name  Suite name.
 * @param count Number of test cases in the suite.
 */
inline void suite_header(const char* name, int count) {
    Log::writeLine("");
    Log::writeLine("======== {} ({} tests) ========", name, count);
}

/**
 * @brief Emit a PASS line: `[n/total] PASS: suite: desc`.
 * @param n     Current test number (1-based).
 * @param total Total cases in the suite.
 * @param suite Suite name.
 * @param desc  Test case description.
 */
inline void ok(int n, int total, const char* suite, const char* desc) {
    Log::writeLine("[{}/{}] PASS: {}: {}", n, total, suite, desc);
}

/**
 * @brief Emit a FAIL line with reason: `[n/total] FAIL: suite::desc`.
 * @param n      Current test number (1-based).
 * @param total  Total cases in the suite.
 * @param suite  Suite name.
 * @param desc   Test case description.
 * @param reason Human-readable failure reason.
 */
inline void fail(int n, int total, const char* suite, const char* desc, const char* reason) {
    Log::writeLine("[{}/{}] FAIL: {}::{}", n, total, suite, desc);
    Log::writeLine("         reason: {}", reason);
}

/**
 * @brief Print a final summary: pass/fail counts and overall verdict.
 * @param passed Number of passed cases.
 * @param failed Number of failed cases.
 * @param total  Total cases run.
 */
inline void summary(int passed, int failed, int total) {
    Log::writeLine("-------- Results --------");
    Log::writeLine("{} passed, {} failed, {} total", passed, failed, total);
    failed == 0 ? Log::writeLine("TESTS PASSED") : Log::writeLine("TESTS FAILED");
}

} // namespace Tap

#endif // !__TAP_H__
