// @file test_vcpu.cpp
// @brief Guest execution through the production vectors and context switch.

#include "tests/integration/suite.h"
#include "tests/integration/guest/binary.h"
#include "core/vmm/esr/esr.h"
#include "core/vcpu/vcpu.h"
#include "core/mm/mmu/guestMmu/guestMmu.h"
#include <iterator>

namespace {
alignas(16) uint8_t guestStack[256];

void setStack(Vcpu& vcpu) {
    vcpu.GetRegisters().spEl1 = reinterpret_cast<uint64_t>(guestStack) + sizeof(guestStack);
}

bool isHvc(VcpuExit exit, uint64_t immediate) {
    return exit.reason == ExitReason::SYNC &&
            GetEsrEc(exit.syndrome) == EsrEc::HVC_AARCH64 &&
            GetEsrIss(exit.syndrome) == immediate;
}

bool guestAbortCapturesAddresses() {
    test::Binary binary { "tests/abort.bin" };
    Vcpu vcpu { binary.GetEntry() };
    setStack(vcpu);
    uint64_t oldVttbr;
    uint64_t oldVtcr;
    uint64_t oldHcr;
    asm volatile("mrs %0, vttbr_el2\n"
                 "mrs %1, vtcr_el2\n"
                 "mrs %2, hcr_el2" : "=r"(oldVttbr), "=r"(oldVtcr), "=r"(oldHcr));
    const uint64_t block { binary.GetEntry() & ~(SIZE_2MB - 1) };
    GuestMmu mmu { block, block, SIZE_2MB, MmioMap {} };
    mmu.Enable(7);
    const VcpuExit exit { vcpu.Run() };
    asm volatile("msr vttbr_el2, %0\n"
                 "msr vtcr_el2, %1\n"
                 "msr hcr_el2, %2\n"
                 "isb\n"
                 "tlbi vmalls12e1is\n"
                 "dsb ish\n"
                 "isb" :: "r"(oldVttbr), "r"(oldVtcr), "r"(oldHcr) : "memory");
    return exit.reason == ExitReason::SYNC &&
            GetEsrEc(exit.syndrome) == EsrEc::DATA_ABORT_LOWER &&
            exit.far == 0xDEAD0000 && exit.hpfar != 0;
}

bool guestReturnsToCaller() {
    test::Binary binary { "tests/vcpu.bin" };
    Vcpu vcpu { binary.GetEntry() };
    setStack(vcpu);
    return isHvc(vcpu.Run(), 0x42);
}

bool guestRegistersSurviveExit() {
    test::Binary binary { "tests/vcpu.bin" };
    Vcpu vcpu { binary.GetEntry() };
    setStack(vcpu);
    auto& registers { vcpu.GetRegisters() };
    for (size_t i {}; i < registers.x.size(); ++i) {
        registers.x[i] = 0x1000 + i;
    }
    if (!isHvc(vcpu.Run(), 0x42)) return false;
    if (registers.x[0] != 0x1234 || registers.x[5] != 0xBEEF) return false;
    if (registers.pc != registers.x[6]) return false;
    if (registers.spEl1 != registers.x[7] || registers.spEl0 != 0x1350) return false;
    for (size_t i {}; i < registers.x.size(); ++i) {
        if (i == 0 || (i >= 5 && i <= 9)) continue;
        if (registers.x[i] != 0x1000 + i) return false;
    }
    return (registers.pstate & 0x3CF) == 0x3C5;
}

bool guestResumesSavedState() {
    test::Binary binary { "tests/vcpu.bin" };
    Vcpu vcpu { binary.GetEntry() };
    setStack(vcpu);
    if (!isHvc(vcpu.Run(), 0x42)) return false;
    asm volatile("msr tpidr_el1, xzr" ::: "memory");
    vcpu.GetRegisters().x[12] = 41;
    if (!isHvc(vcpu.Run(), 0x43)) return false;
    const auto& registers { vcpu.GetRegisters() };
    return registers.x[10] == 0x2468 && registers.x[11] == 0x1350 && registers.x[12] == 42;
}

bool repeatedExitsKeepHostStack() {
    test::Binary binary { "tests/vcpu.bin" };
    Vcpu vcpu { binary.GetEntry() };
    setStack(vcpu);
    if (!isHvc(vcpu.Run(), 0x42)) return false;
    uintptr_t before;
    asm volatile("mov %0, sp" : "=r"(before));
    for (size_t i {}; i < 1024; ++i) {
        if (!isHvc(vcpu.Run(), 0x43)) return false;
    }
    uintptr_t after;
    asm volatile("mov %0, sp" : "=r"(after));
    return before == after && vcpu.GetRegisters().x[12] == 1024;
}

bool hostControlStateSurvivesRun() {
    test::Binary binary { "tests/vcpu.bin" };
    Vcpu vcpu { binary.GetEntry() };
    setStack(vcpu);
    uint64_t savedTpidr;
    uint64_t savedSp;
    uint64_t beforeDaif;
    asm volatile("mrs %0, tpidr_el2\n"
                 "mrs %1, sp_el0\n"
                 "mrs %2, daif" : "=r"(savedTpidr), "=r"(savedSp), "=r"(beforeDaif));
    constexpr uint64_t SENTINEL { 0xABCD };
    asm volatile("msr tpidr_el2, %0" :: "r"(SENTINEL) : "memory");
    const VcpuExit exit { vcpu.Run() };
    uint64_t afterTpidr;
    uint64_t afterSp;
    uint64_t afterDaif;
    asm volatile("mrs %0, tpidr_el2\n"
                 "mrs %1, sp_el0\n"
                 "mrs %2, daif" : "=r"(afterTpidr), "=r"(afterSp), "=r"(afterDaif));
    asm volatile("msr tpidr_el2, %0" :: "r"(savedTpidr) : "memory");
    return isHvc(exit, 0x42) && afterTpidr == SENTINEL &&
            afterSp == savedSp && afterDaif == beforeDaif;
}

extern "C" bool test_vcpu_host_registers(Vcpu* vcpu);

bool hostRegistersSurviveRun() {
    test::Binary binary { "tests/vcpu.bin" };
    Vcpu vcpu { binary.GetEntry() };
    setStack(vcpu);
    return test_vcpu_host_registers(&vcpu);
}

const TestCase cases[] {
    { "guest_returns_to_caller", guestReturnsToCaller },
    { "guest_abort_captures_addresses", guestAbortCapturesAddresses },
    { "guest_registers_survive_exit", guestRegistersSurviveExit },
    { "guest_resumes_saved_state", guestResumesSavedState },
    { "repeated_exits_keep_host_stack", repeatedExitsKeepHostStack },
    { "host_control_state_survives_run", hostControlStateSurvivesRun },
    { "host_registers_survive_run", hostRegistersSurviveRun },
};
const TestSuite suite { "VcpuHarness", cases, std::size(cases) };
}

REGISTER_SUITE(suite);
