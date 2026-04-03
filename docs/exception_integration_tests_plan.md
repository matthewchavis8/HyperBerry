# Plan: Exception & Vector Table Integration Tests (Dynamic VBAR Swap)

## Context
The integration test suite has UART smoke tests but nothing that exercises the exception path. We need to prove that VBAR_EL2 is correctly aligned, the vector table routes EL2 sync exceptions to the right slot, ESR_EL2 carries the correct EC for a known exception, and ELR_EL2 points to the faulting instruction.

**Approach: dynamic VBAR_EL2 swap — zero production code changes.**
Modeled after kvm-unit-tests and TFTF: install a test-only vector table by writing its address into VBAR_EL2, trigger a `brk #0`, capture state in the test handler, restore the production VBAR, assert results.

---

## New Files (all under `tests/integration/exceptions_hw/`)

### `test_vectors.S`
A second vector table that lives entirely in the test directory. Aligned to 0x800. Only the Group 2 sync slot (offset 0x200 — EL2 with SP_ELx, the slot a `brk #0` from EL2 lands in) routes to a real handler. Everything else branches to a test-only spin stub.

```asm
.section .text.test_vectors
.balign 0x800
.global test_vectors

test_vectors:
    // Group 1: SP_EL0 (stubbed)
    .balign 0x80; b test_unhandled_entry
    .balign 0x80; b test_unhandled_entry
    .balign 0x80; b test_unhandled_entry
    .balign 0x80; b test_unhandled_entry

    // Group 2: SP_ELx — active EL2 handlers
    .balign 0x80; b test_el2_sync_entry   // BRK #0 from EL2 lands here
    .balign 0x80; b test_unhandled_entry
    .balign 0x80; b test_unhandled_entry
    .balign 0x80; b test_unhandled_entry

    // Group 3: Lower EL AArch64 (stubbed)
    .balign 0x80; b test_unhandled_entry
    .balign 0x80; b test_unhandled_entry
    .balign 0x80; b test_unhandled_entry
    .balign 0x80; b test_unhandled_entry

    // Group 4: Lower EL AArch32 (stubbed)
    .balign 0x80; b test_unhandled_entry
    .balign 0x80; b test_unhandled_entry
    .balign 0x80; b test_unhandled_entry
    .balign 0x80; b test_unhandled_entry

// Minimal save/restore + call to C handler
test_el2_sync_entry:
    sub  sp, sp, #272
    stp  x0,  x1,  [sp, #0]
    stp  x2,  x3,  [sp, #16]
    stp  x4,  x5,  [sp, #32]
    stp  x6,  x7,  [sp, #48]
    stp  x8,  x9,  [sp, #64]
    stp  x10, x11, [sp, #80]
    stp  x12, x13, [sp, #96]
    stp  x14, x15, [sp, #112]
    stp  x16, x17, [sp, #128]
    stp  x18, x19, [sp, #144]
    stp  x20, x21, [sp, #160]
    stp  x22, x23, [sp, #176]
    stp  x24, x25, [sp, #192]
    stp  x26, x27, [sp, #208]
    stp  x28, x29, [sp, #224]
    str  x30,      [sp, #240]
    mrs  x0, elr_el2
    mrs  x1, spsr_el2
    stp  x0,  x1,  [sp, #248]
    mov  x0, sp              // arg0: pointer to saved frame (same layout as ExceptionContext)
    mrs  x1, esr_el2         // arg1: ESR value
    bl   handle_test_el2_sync
    ldp  x0,  x1,  [sp, #248]
    msr  elr_el2, x0
    msr  spsr_el2, x1
    ldp  x0,  x1,  [sp, #0]
    ldp  x2,  x3,  [sp, #16]
    ldp  x4,  x5,  [sp, #32]
    ldp  x6,  x7,  [sp, #48]
    ldp  x8,  x9,  [sp, #64]
    ldp  x10, x11, [sp, #80]
    ldp  x12, x13, [sp, #96]
    ldp  x14, x15, [sp, #112]
    ldp  x16, x17, [sp, #128]
    ldp  x18, x19, [sp, #144]
    ldp  x20, x21, [sp, #160]
    ldp  x22, x23, [sp, #176]
    ldp  x24, x25, [sp, #192]
    ldp  x26, x27, [sp, #208]
    ldp  x28, x29, [sp, #224]
    ldr  x30,      [sp, #240]
    add  sp, sp, #272
    eret

test_unhandled_entry:
    b .                       // spin — test bug if reached
```

### `test_exception_state.h`
Shared state written by the C handler, read by test cases:
```cpp
#pragma once
#include <stdint.h>
#include <stdbool.h>

struct TestExceptionState {
    bool     called;
    uint64_t esr;
    uint64_t elr;      // address of faulting instruction (before +4 advance)
    uint64_t gpr19;    // spot-check one callee-saved register
};

extern TestExceptionState g_test_ex_state;

// Declared here, defined in test_exception_handler.cpp
extern "C" void handle_test_el2_sync(void* frame, uint64_t esr);
```

### `test_exception_handler.cpp`
```cpp
#include "test_exception_state.h"
#include "core/exceptions/exceptions.h"   // for ExceptionContext layout

TestExceptionState g_test_ex_state;

extern "C" void handle_test_el2_sync(void* frame, uint64_t esr) {
    auto* ctx = reinterpret_cast<ExceptionContext*>(frame);
    g_test_ex_state.called  = true;
    g_test_ex_state.esr     = esr;
    g_test_ex_state.elr     = ctx->elr;     // ELR before advance
    g_test_ex_state.gpr19   = ctx->gpr[19];
    ctx->elr += 4;                          // skip faulting instruction on eret
}
```

### `test_exceptions_hw.cpp`
```cpp
#include "tests/integration/suite.h"
#include "test_exception_state.h"

extern "C" void* test_vectors;  // defined in test_vectors.S

// RAII helper: saves VBAR_EL2, installs test table, restores on scope exit
struct VbarGuard {
    uint64_t saved;
    VbarGuard() {
        asm volatile("mrs %0, vbar_el2" : "=r"(saved));
        asm volatile("msr vbar_el2, %0\nisb" :: "r"(&test_vectors) : "memory");
    }
    ~VbarGuard() {
        asm volatile("msr vbar_el2, %0\nisb" :: "r"(saved) : "memory");
    }
};

static bool test_vbar_aligned() {
    uint64_t vbar;
    asm volatile("mrs %0, vbar_el2" : "=r"(vbar));
    return (vbar != 0) && ((vbar & 0x7FF) == 0);
}

static bool test_brk_fires_handler() {
    g_test_ex_state = {};
    VbarGuard guard;
    asm volatile("brk #0");
    return g_test_ex_state.called;
}

static bool test_brk_esr_ec_correct() {
    g_test_ex_state = {};
    VbarGuard guard;
    asm volatile("brk #0");
    uint64_t ec = (g_test_ex_state.esr >> 26) & 0x3F;
    return ec == 0x3C;  // AArch64 BRK instruction from EL2
}

static bool test_brk_elr_points_to_brk() {
    g_test_ex_state = {};
    VbarGuard guard;
    uint64_t brk_addr;
    asm volatile(
        "adr %0, 1f\n"
        "1: brk #0\n"
        : "=r"(brk_addr)
    );
    return g_test_ex_state.elr == brk_addr;
}

static bool test_context_gpr_preserved() {
    g_test_ex_state = {};
    VbarGuard guard;
    asm volatile(
        "mov x19, #0xBEEF\n"
        "brk #0\n"
        ::: "x19"
    );
    return g_test_ex_state.gpr19 == 0xBEEF;
}

static const TestCase kCases[] = {
    {"vbar_aligned",          test_vbar_aligned},
    {"brk_fires_handler",     test_brk_fires_handler},
    {"brk_esr_ec_correct",    test_brk_esr_ec_correct},
    {"brk_elr_points_to_brk", test_brk_elr_points_to_brk},
    {"context_gpr_preserved", test_context_gpr_preserved},
};
static const TestSuite kSuite = {"exceptions_hw", kCases, 5};
REGISTER_SUITE(kSuite);
```

### `CMakeLists.txt`
```cmake
target_sources(hyperberry PRIVATE
    test_vectors.S
    test_exception_handler.cpp
    test_exceptions_hw.cpp
)
```

And in `tests/integration/CMakeLists.txt`, add:
```cmake
add_subdirectory(exceptions_hw)
```

---

## Critical Files

| File | Change |
|------|--------|
| `tests/integration/exceptions_hw/test_vectors.S` | **New** — test-only vector table |
| `tests/integration/exceptions_hw/test_exception_state.h` | **New** — shared state struct |
| `tests/integration/exceptions_hw/test_exception_handler.cpp` | **New** — C handler captures state, advances ELR |
| `tests/integration/exceptions_hw/test_exceptions_hw.cpp` | **New** — 5 test cases + REGISTER_SUITE |
| `tests/integration/exceptions_hw/CMakeLists.txt` | **New** — wires sources |
| `tests/integration/CMakeLists.txt` | **Modify** — add `add_subdirectory(exceptions_hw)` |

**Zero changes to any production file.**

---

## Verification

```bash
just qemu-integration   # or: cmake -DINTEGRATION_TEST=ON -DBOARD=qemu && ninja

# Expected UART TAP output:
# === exceptions_hw ===
# ok 1 - vbar_aligned
# ok 2 - brk_fires_handler
# ok 3 - brk_esr_ec_correct
# ok 4 - brk_elr_points_to_brk
# ok 5 - context_gpr_preserved
# passed: 5 / 5
```

All five tests must pass without hanging (no spin in `test_unhandled_entry`, execution resumes after each `brk #0`).
