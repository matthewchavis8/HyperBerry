// @file test_vmm.cpp
// @brief Integration tests for EL2 exception vector table routing.

#include "tests/integration/suite.h"
#include "trapState.h"

// Holds the current state of EL2 vector table during testing
static TestExceptionState vecBarState;

// @brief Test only EL2 sync handler.
//
// Called from test_el2_sync_entry in test_vectors.S. Captures ESR, ELR and x19
// into vecBarState, then advances ELR by 4 so eret skips the faulting BRK. A64
// BRK is a fixed 4 byte encoding, and EL2 always runs AArch64.
// @param frame Frame saved by test_el2_sync_entry.
// @param esr ESR_EL2 at the trap.
// @return Nothing.
extern "C" void handle_test_el2_sync(void* frame, uint64_t esr) {
    uint64_t* ctx = reinterpret_cast<uint64_t*>(frame);

    vecBarState.isCalled = true;
    // Store the exception syndrome register to see what triggered the crash
    vecBarState.esr = esr;
    // ELR/SPSR live after x0-x30 in the saved exception frame.
    vecBarState.elr = ctx[31];
    // A callee saved register catches a misaligned or corrupted frame.
    vecBarState.gpr19 = ctx[19];
    // Skip the faulting BRK so eret resumes at the next instruction.
    ctx[31] += 4;
}

// Symbol defined from test_vectors.S
extern "C" char test_vectors[];

// @brief RAII guard that swaps VBAR_EL2 for the test table.
//
// The constructor saves VBAR_EL2 and installs test_vectors; the destructor puts
// the original back. The isb makes sure the new table is live before a trap.
class VbarGuard {
private:
    uint64_t m_saved;

public:
    VbarGuard() {
        asm volatile("mrs %0, vbar_el2" : "=r"(m_saved));
        uint64_t test_vbar = reinterpret_cast<uint64_t>(test_vectors);
        asm volatile("msr vbar_el2, %0\n"
                     "isb" ::"r"(test_vbar)
                : "memory");
    }

    ~VbarGuard() {
        asm volatile("msr vbar_el2, %0\n"
                     "isb" ::"r"(m_saved)
                : "memory");
    }
};


// @test Production VBAR_EL2 is nonzero and aligned to 0x800.
//
// Runs before the test table is installed.
// @return true when VBAR_EL2 is set and bits [10:0] are clear.
static bool test_vbar_aligned() {
    uint64_t vbar;
    asm volatile("mrs %0, vbar_el2" : "=r"(vbar));
    return (vbar != 0) && ((vbar & 0x7FF) == 0);
}

// @test BRK #0 reaches the test handler.
// @return true when handle_test_el2_sync ran.
static bool test_brk_fires_handler() {
    vecBarState = {};
    VbarGuard guard;
    asm volatile("brk #0");
    return vecBarState.isCalled;
}

// @test ESR_EL2.EC reads 0x3C, BRK from AArch64.
// @return true when the captured EC is 0x3C.
static bool test_brk_esr_ec_correct() {
    vecBarState = {};
    VbarGuard guard;
    asm volatile("brk #0");

    uint64_t ec = (vecBarState.esr >> 26) & 0x3F;
    return ec == 0x3C;
}

// @test ELR_EL2 points at the BRK.
//
// adr captures the BRK address; the handler records ELR before advancing it.
// @return true when the captured ELR matches.
static bool test_brk_elr_points_to_brk() {
    vecBarState = {};
    VbarGuard guard;

    uint64_t brk_addr;
    asm volatile("adr %0, 1f\n"
                 "1: brk #0\n"
            : "=r"(brk_addr));

    return vecBarState.elr == brk_addr;
}

// @test x19 survives the frame save.
//
// Loads 0xBEEF into x19 before the BRK and checks the handler read it back from
// the saved frame.
// @return true when the saved x19 is 0xBEEF.
static bool test_context_gpr_preserved() {
    vecBarState = {};
    VbarGuard guard;

    asm volatile("mov x19, #0xBEEF\n"
                 "brk #0\n" ::
                         : "x19");

    return vecBarState.gpr19 == 0xBEEF;
}


static const TestCase kCases[] = {
    { "vbar_aligned", test_vbar_aligned },
    { "brk_fires_handler", test_brk_fires_handler },
    { "brk_esr_ec_correct", test_brk_esr_ec_correct },
    { "brk_elr_points_to_brk", test_brk_elr_points_to_brk },
    { "context_gpr_preserved", test_context_gpr_preserved },
};

static const TestSuite kSuite = { "ExceptionHarness", kCases, 5 };

REGISTER_SUITE(kSuite);
