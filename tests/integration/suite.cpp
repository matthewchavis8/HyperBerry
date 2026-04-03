/**
 * @file suite.cpp
 * @brief Integration test runner — walks the `.hyperberry_tests` linker section.
 *
 * At boot (when INTEGRATION_TEST is defined), hmain() calls
 * TestRunner::run_all(). The runner iterates every TestSuite pointer
 * placed into the section by REGISTER_SUITE, executes each case,
 * and prints results over UART. Spins forever when finished.
 */

#include "tests/integration/suite.h"
#include "tests/integration/tap/tap.h"

extern "C" const TestSuite* __test_suites_start[];
extern "C" const TestSuite* __test_suites_end[];

namespace TestRunner {

/**
 * @brief Execute every registered integration test suite and print a UART summary.
 *
 * The runner walks the linker-populated `.hyperberry_tests` range, executes
 * each test case in registration order, and never returns.
 */
void run_all() {
  // Get the starting/ending address of the test_suite where the linker script laid it out in memory
  const TestSuite* const* begin = __test_suites_start;
  const TestSuite* const* end = __test_suites_end;

  int total_passed {};
  int total_failed {};

  // Here we walk each individual test suite until we hit the end of the test address
  for (const TestSuite* const* it = begin; it != end; ++it) {
    const TestSuite* suite = *it;

    Tap::suite_header(suite->name, suite->count);

    for (int i = 0; i < suite->count; i++) {
      const TestCase& tc = suite->cases[i];
      int num = i + 1;

      if (tc.fn()) {
        Tap::ok(num, suite->count, suite->name, tc.name);
        total_passed++;
      } else {
        Tap::fail(num, suite->count, suite->name, tc.name,
                  "test returned false");
        total_failed++;
      }
    }
  }

  Tap::summary(total_passed, total_failed, total_passed + total_failed);

  for (;;) {}
}

} // namespace TestRunner
