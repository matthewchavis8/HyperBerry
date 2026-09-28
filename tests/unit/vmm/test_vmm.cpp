// @file test_vmm.cpp
// @brief Guest exit policy operates only on saved state.

#include <gtest/gtest.h>
#include "core/vcpu/vcpu.h"
#include "core/vmm/vmm.h"
#include "lib/panic/panic.h"

[[noreturn]] void HvPanic(const char* message, const std::array<uint64_t, 31>&) {
    HvPanic(message);
}

namespace {
VcpuExit syncExit(EsrEc exception) {
    return { ExitReason::SYNC, static_cast<uint64_t>(exception) << 26 };
}
}

TEST(GuestExit, HvcUpdatesResultWithoutSkippingNextInstruction) {
    GuestRegisters registers { .pc = 0x400004 };
    registers.x[0] = 0x84000000;
    HandleGuestExit(registers, syncExit(EsrEc::HVC_AARCH64));
    EXPECT_EQ(registers.x[0], 0x10000);
    EXPECT_EQ(registers.pc, 0x400004);
}

TEST(GuestExit, UnknownHvcReturnsNotSupported) {
    GuestRegisters registers { .pc = 0x400004 };
    registers.x[0] = 0xDEADBEEF;
    HandleGuestExit(registers, syncExit(EsrEc::HVC_AARCH64));
    EXPECT_EQ(registers.x[0], UINT64_MAX);
    EXPECT_EQ(registers.pc, 0x400004);
}

TEST(GuestExit, SmcReturnsNotSupportedAndSkipsTrappedInstruction) {
    GuestRegisters registers { .pc = 0x400000 };
    registers.x[0] = 0x84000000;
    registers.x[1] = 123;
    HandleGuestExit(registers, syncExit(EsrEc::SMC_AARCH64));
    EXPECT_EQ(registers.x[0], UINT64_MAX);
    EXPECT_EQ(registers.x[1], 123);
    EXPECT_EQ(registers.pc, 0x400004);
}

TEST(GuestExit, InterruptsPreserveGuestRegisters) {
    GuestRegisters registers { .pc = 0x400000 };
    registers.x[0] = 123;
    for (const auto reason : { ExitReason::IRQ, ExitReason::FIQ }) {
        HandleGuestExit(registers, { reason, 0 });
        EXPECT_EQ(registers.x[0], 123);
        EXPECT_EQ(registers.pc, 0x400000);
    }
}

TEST(GuestExit, FatalExitsDoNotResume) {
    GuestRegisters registers {};
    EXPECT_DEATH(HandleGuestExit(registers, { ExitReason::SERROR, 0 }), "");
    EXPECT_DEATH(HandleGuestExit(registers, syncExit(EsrEc::DATA_ABORT_LOWER)), "");
    EXPECT_DEATH(HandleGuestExit(registers, syncExit(EsrEc::UNKNOWN)), "");
    registers.x[0] = 0x84000008;
    EXPECT_DEATH(HandleGuestExit(registers, syncExit(EsrEc::HVC_AARCH64)), "");
    registers.x[0] = 0x84000009;
    EXPECT_DEATH(HandleGuestExit(registers, syncExit(EsrEc::HVC_AARCH64)), "");
}
