/**
 * @file test_exceptions.cpp
 * @brief Integration tests for EL2 exception vector table routing.
 */

#include "tests/integration/suite.h"
#include "exceptionState.h"
#include "core/exceptions/exceptions.h"

// Holds the current state of EL2 vector table during testing
static TestExceptionState vecBarState;

/**
 * @brief Test-only EL2 sync exception handler.
 *
 * Called from test_el2_sync_entry in test_vectors.S.
 * Captures ESR, ELR, and a spot-check GPR into g_test_ex_state,
 * then advances ELR by 4 so eret skips the faulting BRK instruction.
 *
 * The +4 advance is safe because BRK is a fixed 4-byte A64 encoding
 * and we are always in AArch64 at EL2.
 */
extern "C" void handle_test_el2_sync(void* frame, uint64_t esr) {
    ExceptionContext* ctx = reinterpret_cast<ExceptionContext*>(frame);

    vecBarState.isCalled = true;
    // Store the exception syndrome register to see what triggered the crash
    vecBarState.esr      = esr;
    // Capture exception link register before advancing
    vecBarState.elr      = ctx->elr;
    // Grab the first non volatile register to make sure allignment or stack corruption did not occur
    vecBarState.gpr19    = ctx->gpr[19]; 
    // Skip the faulting BRK so eret resumes at the next instruction.
    ctx->elr += 4;
}

// Symbol defined from test_vectors.S
extern "C" char test_vectors[];

/**
 * @brief RAII guard for temporarily swapping VBAR_EL2.
 *
 * Constructor saves current VBAR_EL2 and installs the test vector table.
 * Destructor restores the original VBAR_EL2. ISB ensures the pipeline
 * sees the new table before any exception can fire.
 */
class VbarGuard {
  private:
    uint64_t m_saved;

  public:
    VbarGuard() {
        asm volatile("mrs %0, vbar_el2" : "=r"(m_saved));
        uint64_t test_vbar = reinterpret_cast<uint64_t>(test_vectors);
        asm volatile(
            "msr vbar_el2, %0\n"
            "isb"
            :: "r"(test_vbar)
            : "memory"
        );
    }

    ~VbarGuard() {
        asm volatile(
            "msr vbar_el2, %0\n"
            "isb"
            :: "r"(m_saved)
            : "memory"
        );
    }
};


/**
 * @test Verify production VBAR_EL2 is non-zero and 0x800-aligned.
 *
 * ARMv8-A requires VBAR alignment to 0x800 (bits [10:0] must be zero,
 * but the minimum meaningful alignment is 11 bits = 0x800).
 * This runs *before* installing the test table.
 */
static bool test_vbar_aligned() {
    uint64_t vbar;
    asm volatile("mrs %0, vbar_el2" : "=r"(vbar));
    return (vbar != 0) && ((vbar & 0x7FF) == 0);
}

/**
 * @test Trigger BRK #0 and verify the test handler was called.
 *
 * If the vector table is wired correctly, test_el2_sync_entry runs,
 * calls handle_test_el2_sync, sets g_test_ex_state.called = true,
 * advances ELR, and erets back here.
 */
static bool test_brk_fires_handler() {
    vecBarState = {};
    VbarGuard guard;
    asm volatile("brk #0");
    return vecBarState.isCalled;
}

/**
 * @test Verify ESR_EL2 exception class is 0x3C (BRK from AArch64).
 *
 * ESR_EL2 bits [31:26] hold the Exception Class (EC).
 * EC = 0x3C means "BRK instruction execution in AArch64 state"
 * when taken from the current EL.
 */
static bool test_brk_esr_ec_correct() {
    vecBarState = {};
    VbarGuard guard;
    asm volatile("brk #0");

    uint64_t ec = (vecBarState.esr >> 26) & 0x3F;
    return ec == 0x3C;
}

/**
 * @test Verify ELR_EL2 points to the BRK instruction.
 *
 * Uses `adr` to capture the address of the BRK before executing it.
 * The handler records ELR before advancing it, so we compare against
 * the pre-advance value.
 */
static bool test_brk_elr_points_to_brk() {
    vecBarState = {};
    VbarGuard guard;

    uint64_t brk_addr;
    asm volatile(
        "adr %0, 1f\n"
        "1: brk #0\n"
        : "=r"(brk_addr)
    );

    return vecBarState.elr == brk_addr;
}

/**
 * @test Verify callee-saved register x19 round-trips through context save.
 *
 * Loads a known sentinel (0xBEEF) into x19 before BRK, then checks
 * that the handler read the same value from the saved frame.
 * Validates that the test_vectors.S context save covers callee-saved regs.
 */
static bool test_context_gpr_preserved() {
    vecBarState = {};
    VbarGuard guard;

    asm volatile(
        "mov x19, #0xBEEF\n"
        "brk #0\n"
        ::: "x19"
    );

    return vecBarState.gpr19 == 0xBEEF;
}


static const TestCase kCases[] = {
    {"vbar_aligned",           test_vbar_aligned},
    {"brk_fires_handler",      test_brk_fires_handler},
    {"brk_esr_ec_correct",     test_brk_esr_ec_correct},
    {"brk_elr_points_to_brk",  test_brk_elr_points_to_brk},
    {"context_gpr_preserved",  test_context_gpr_preserved},
};

static const TestSuite kSuite = {"ExceptionHarness", kCases, 5};

REGISTER_SUITE(kSuite);
