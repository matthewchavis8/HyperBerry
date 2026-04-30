/**
 * @file suite.h
 * @brief Integration test suite types and linker-section auto-registration.
 *
 * Defines TestCase, TestSuite, and the REGISTER_SUITE macro which places
 * a pointer into the `.hyperberry_tests` linker section. The test runner
 * walks that section at boot to discover and execute all registered suites.
 */

#ifndef __SUITE_H__
#define __SUITE_H__

#include "core/dtb/dtb.h"

/**
 * @brief A single test case: a name and a function that returns true on pass.
 */
struct TestCase {
  /** Human-readable test case name printed in UART output. */
  const char* name;
  /** Test body. Returns true on pass, false on failure. */
  bool (*fn)();
};

/**
 * @brief A named collection of test cases.
 */
struct TestSuite {
  /** Suite name printed in the banner and case output. */
  const char*     name;
  /** Pointer to the suite's static array of test cases. */
  const TestCase* cases;
  /** Number of entries in `cases`. */
  int             count;
};

/**
 * @brief Register a TestSuite for automatic discovery by the runner.
 *
 * Places a pointer to the suite into the `.hyperberry_tests` linker section.
 * The `used` attribute prevents the compiler from discarding it as unused.
 */
#define REGISTER_SUITE(s)                               \
  __attribute__((section(".hyperberry_tests"), used))    \
  static const TestSuite* __reg_##s = &s

namespace TestRunner {
/**
 * @brief Save the boot-time memory map for integration tests that need it.
 */
void setBootContext(const MemoryMap& map);

/**
 * @brief Return the boot-time memory map captured before the suite started.
 */
const MemoryMap& bootMemoryMap();

/**
 * @brief Walk all registered suites and run every test case.
 */
void run_all();
}

#endif // !__SUITE_H__
