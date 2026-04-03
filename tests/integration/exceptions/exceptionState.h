/**
 * @file test_exception_state.h
 * @brief Shared state between the test exception handler and test cases.
 *
 * The handler writes into g_test_ex_state; each test case zeroes it
 * before triggering BRK #0, then reads the captured values to assert.
 */

#ifndef __EXCEPTION_STATE_H__
#define __EXCEPTION_STATE_H__

#include <stdint.h>

struct TestExceptionState {
    bool     isCalled;    ///< Handler was entered
    uint64_t esr;       ///< Raw ESR_EL2 value
    uint64_t elr;       ///< ELR_EL2 *before* the +4 advance
    uint64_t gpr19;     ///< Stores the x19 register for testing
};


#endif // !__EXCEPTION_STATE_H__
