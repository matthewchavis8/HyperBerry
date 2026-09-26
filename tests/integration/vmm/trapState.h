// @file trapState.h
// @brief Shared state between the test exception handler and test cases.
//
// The handler writes into vecBarState; each test case zeroes it before
// triggering BRK #0, then reads the captured values.

#ifndef __TRAP_STATE_H__
#define __TRAP_STATE_H__

#include <cstdint>

struct TestExceptionState {
    bool isCalled;  // Handler was entered
    uint64_t esr;   // Raw ESR_EL2 value
    uint64_t elr;   // ELR_EL2 before the handler advances it
    uint64_t gpr19; // x19, to catch a frame saved to the wrong slot
};


#endif // !__TRAP_STATE_H__
