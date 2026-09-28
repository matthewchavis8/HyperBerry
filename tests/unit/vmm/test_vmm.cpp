#include <gtest/gtest.h>
#include "core/vmm/vmm.h"
#include "core/vmm/esr/esr.h"
#include "tests/unit/vcpu/backend.h"

namespace {
VmConfig config() { return { "test VM", 0, 0x40000000, 0x200000, 2, 0x200000, 0x1FF000 }; }
VcpuExit syncExit(EsrEc ec, uint64_t iss = 0) {
    return { ExitReason::SYNC, (static_cast<uint64_t>(ec) << 26) | iss };
}

class VmmTest : public testing::Test {
protected:
    void TearDown() override { vcpuTest::run = {}; }
};
}

TEST_F(VmmTest, PsciOffShutsDownAndCannotRunAgain) {
    Vmm vmm { config(), MmioMap {} };
    EXPECT_EQ(vmm.GetState(), VmState::READY);
    vcpuTest::run = [](Vcpu& vcpu) {
        vcpu.GetRegisters().x[0] = 0x84000008;
        return syncExit(EsrEc::HVC_AARCH64);
    };
    EXPECT_EQ(vmm.Run().value(), VmState::SHUTDOWN);
    EXPECT_EQ(vmm.GetState(), VmState::SHUTDOWN);
    EXPECT_EQ(vmm.GetLastExit().reason, ExitReason::SYNC);
    EXPECT_EQ(vmm.Run().error(), RunError::INVALID_STATE);
}

TEST_F(VmmTest, PsciResetReportsRequest) {
    Vmm vmm { config(), MmioMap {} };
    vcpuTest::run = [](Vcpu& vcpu) {
        vcpu.GetRegisters().x[0] = 0x84000009;
        return syncExit(EsrEc::HVC_AARCH64);
    };
    EXPECT_EQ(vmm.Run().value(), VmState::RESET_REQUESTED);
}

TEST_F(VmmTest, UnsupportedHvcAndSmcResumeGuest) {
    Vmm vmm { config(), MmioMap {} };
    size_t runs {};
    vcpuTest::run = [&](Vcpu& vcpu) {
        auto& registers { vcpu.GetRegisters() };
        if (runs++ == 0) {
            registers.x[0] = 0x84000000;
            return syncExit(EsrEc::HVC_AARCH64, 1);
        }
        if (runs == 2) {
            EXPECT_EQ(registers.x[0], UINT64_MAX);
            registers.pc = 0x200000;
            return syncExit(EsrEc::SMC_AARCH64);
        }
        EXPECT_EQ(registers.pc, 0x200004);
        EXPECT_EQ(registers.x[0], UINT64_MAX);
        registers.x[0] = 0x84000008;
        return syncExit(EsrEc::HVC_AARCH64);
    };
    EXPECT_EQ(vmm.Run().value(), VmState::SHUTDOWN);
    EXPECT_EQ(runs, 3);
}

TEST_F(VmmTest, GuestAbortFaultsWithoutPanickingHost) {
    Vmm vmm { config(), MmioMap {} };
    vcpuTest::run = [](Vcpu&) {
        VcpuExit exit { syncExit(EsrEc::DATA_ABORT_LOWER) };
        exit.far = 0x1234;
        exit.hpfar = 0x5678;
        return exit;
    };
    EXPECT_EQ(vmm.Run().value(), VmState::FAULTED);
    EXPECT_EQ(vmm.GetLastExit().far, 0x1234);
    EXPECT_EQ(vmm.GetLastExit().hpfar, 0x5678);
}

TEST_F(VmmTest, GuestSErrorFaultsWithoutPanickingHost) {
    Vmm vmm { config(), MmioMap {} };
    vcpuTest::run = [](Vcpu&) { return VcpuExit { ExitReason::SERROR, 0 }; };
    EXPECT_EQ(vmm.Run().value(), VmState::FAULTED);
}
