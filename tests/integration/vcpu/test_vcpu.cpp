// @file test_vcpu.cpp
// @brief Integration tests for the Vcpu subsystem on AArch64.

#include "tests/integration/suite.h"
#include "tests/integration/guest/binary.h"
#include "core/vmm/esr.h"
#include "core/vcpu/vcpu.h"

namespace {
struct GuestExitCapture {
    bool isCalled;
    Vcpu* vcpu;
    uint64_t esr;
};

constexpr uint64_t kGuestCallId { 0x1234ULL };
constexpr uint64_t kGuestScratch { 0xBEEFULL };

alignas(16) uint8_t gGuestStack[256];
GuestExitCapture gGuestExit;

extern "C" char test_vcpu_vectors[];

extern "C" void handle_test_vcpu_guest_exit(Vcpu* vcpu, uint64_t esr) {
    gGuestExit.isCalled = true;
    gGuestExit.vcpu = vcpu;
    gGuestExit.esr = esr;
}

uint64_t installTestVbar() {
    uint64_t saved { 0 };
    asm volatile("mrs %0, vbar_el2" : "=r"(saved));
    uint64_t testVbar { reinterpret_cast<uint64_t>(test_vcpu_vectors) };
    asm volatile("msr vbar_el2, %0\n"
                 "isb" ::"r"(testVbar)
            : "memory");
    return saved;
}

void restoreVbar(uint64_t saved) {
    asm volatile("msr vbar_el2, %0\n"
                 "isb" ::"r"(saved)
            : "memory");
}

bool enterGuestAndCapture(Vcpu& vcpu) {
    gGuestExit = {};

    if (!vcpu.GetElr()) return false;
    vcpu.SetGuestSp(reinterpret_cast<uint64_t>(gGuestStack) + sizeof(gGuestStack));

    uint64_t savedVbar { installTestVbar() };
    vcpu_enter(&vcpu);
    restoreVbar(savedVbar);
    return gGuestExit.isCalled;
}
} // namespace

static bool test_vcpu_is_standard_layout() {
    return __is_standard_layout(Vcpu);
}

static bool test_vcpu_hvctx_size_matches_asm() {
    return sizeof(HvContext) == VCPU_HVCTX_SIZE;
}

static bool test_vcpu_hvctx_offset_matches_asm() {
    return VcpuLayoutAccess::GetHvCtxOffset() == VCPU_HVCTX_OFFSET;
}

static bool test_vcpu_gpr_round_trip() {
    Vcpu vcpu { 0x40000000ULL };
    vcpu.SetGpReg(VCPU_GPREG_X5, 0xCAFEBABEULL);
    return vcpu.GetGpReg(VCPU_GPREG_X5) == 0xCAFEBABEULL;
}

static bool test_vcpu_tpidr_el2_is_accessible() {
    constexpr uint64_t kSentinel { 0xDEADBEEFCAFEULL };
    uint64_t readBack { 0 };

    asm volatile("msr tpidr_el2, %1\n"
                 "mrs %0, tpidr_el2\n"
            : "=r"(readBack)
            : "r"(kSentinel)
            : "memory");

    return readBack == kSentinel;
}

static bool test_vcpu_guest_exit_returns_to_caller() {
    test::Binary binary { "tests/vcpu.bin" };
    Vcpu vcpu { binary.GetEntry() };
    return enterGuestAndCapture(vcpu) && gGuestExit.vcpu == &vcpu &&
            Vcpu::GetCurrentVcpu() == &vcpu;
}

static bool test_vcpu_guest_exit_captures_hvc_esr() {
    test::Binary binary { "tests/vcpu.bin" };
    Vcpu vcpu { binary.GetEntry() };
    if (!enterGuestAndCapture(vcpu)) {
        return false;
    }

    return GetEsrEc(gGuestExit.esr) == EsrEc::HVC_AARCH64;
}

static bool test_vcpu_guest_exit_saves_guest_pc() {
    test::Binary binary { "tests/vcpu.bin" };
    Vcpu vcpu { binary.GetEntry() };
    if (!enterGuestAndCapture(vcpu)) {
        return false;
    }

    return vcpu.GetElr() == vcpu.GetGpReg(VCPU_GPREG_X6);
}

static bool test_vcpu_guest_exit_saves_guest_gprs() {
    test::Binary binary { "tests/vcpu.bin" };
    Vcpu vcpu { binary.GetEntry() };
    if (!enterGuestAndCapture(vcpu)) {
        return false;
    }

    return vcpu.GetGpReg(VCPU_GPREG_X0) == kGuestCallId &&
            vcpu.GetGpReg(VCPU_GPREG_X5) == kGuestScratch;
}

static const TestCase kVcpuCases[] {
    { "vcpu_is_standard_layout", test_vcpu_is_standard_layout },
    { "vcpu_hvctx_size_matches_asm", test_vcpu_hvctx_size_matches_asm },
    { "vcpu_hvctx_offset_matches_asm", test_vcpu_hvctx_offset_matches_asm },
    { "vcpu_gpr_round_trip", test_vcpu_gpr_round_trip },
    { "vcpu_tpidr_el2_is_accessible", test_vcpu_tpidr_el2_is_accessible },
    { "vcpu_guest_exit_returns_to_caller", test_vcpu_guest_exit_returns_to_caller },
    { "vcpu_guest_exit_captures_hvc_esr", test_vcpu_guest_exit_captures_hvc_esr },
    { "vcpu_guest_exit_saves_guest_pc", test_vcpu_guest_exit_saves_guest_pc },
    { "vcpu_guest_exit_saves_guest_gprs", test_vcpu_guest_exit_saves_guest_gprs },
};

static const TestSuite kVcpuSuite {
    "VcpuHarness",
    kVcpuCases,
    9,
};

REGISTER_SUITE(kVcpuSuite);
