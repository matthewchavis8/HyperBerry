// @file test_vm.cpp
// @brief VM mappings, boot state, and exit handling with hardware entry replaced.

#include <gtest/gtest.h>
#include "core/vm/vm.h"
#include "core/vmm/vmm.h"
#include "core/vmm/esr/esr.h"
#include "tests/unit/vcpu/backend.h"
#include <type_traits>

namespace {
uint64_t guestIpa;
uint64_t guestHostPa;
uint64_t guestSize;
uint8_t enabledVmid;
uint32_t deviceCount;

struct GuestStopped {};

class VmTest : public testing::Test {
protected:
    void SetUp() override {
        guestIpa = 0;
        guestHostPa = 0;
        guestSize = 0;
        enabledVmid = 0;
        deviceCount = 0;
    }

    void TearDown() override { vcpuTest::run = {}; }

    static VmConfig config() {
        return { "test VM", 0, 0x40000000, 0x200000, 2, 0x200000, 0x1FF000 };
    }
};
} // namespace

GuestMmu::GuestMmu(
        uint64_t ipaBase, uint64_t hostPaBase, uint64_t sizeBytes, const MmioMap& devices) :
            m_table { nullptr, 0, 0 } {
    guestIpa = ipaBase;
    guestHostPa = hostPaBase;
    guestSize = sizeBytes;
    deviceCount = devices.GetCount();
}

void GuestMmu::Enable(uint8_t vmid) {
    enabledVmid = vmid;
}

void GuestMmu::MapBlock(uint64_t, uint64_t, bool) {}
void GuestMmu::TlbFlushAllGuest() {}

static_assert(!std::is_copy_constructible_v<Vm>);
static_assert(!std::is_move_constructible_v<Vm>);

TEST_F(VmTest, ConstructsGuestAddressSpace) {
    MmioMap devices {};
    devices.AddPages(0x09000000, 0x09000000, 0x1000);
    Vm vm { config(), devices };
    EXPECT_EQ(guestIpa, 0);
    EXPECT_EQ(guestHostPa, 0x40000000);
    EXPECT_EQ(guestSize, 0x200000);
    EXPECT_EQ(deviceCount, 1);
    EXPECT_EQ(enabledVmid, 0);
    EXPECT_EQ(vm.GetName(), "test VM");
    EXPECT_EQ(vm.GetVmId(), 2);
}

TEST_F(VmTest, ActivatesAddressSpaceBeforeEnteringLinux) {
    Vmm vmm { config(), MmioMap {} };
    vcpuTest::run = [](Vcpu& vcpu) -> VcpuExit {
        EXPECT_EQ(enabledVmid, 2);
        const auto& registers { vcpu.GetRegisters() };
        EXPECT_EQ(registers.pc, 0x200000);
        EXPECT_EQ(registers.x[0], 0x1FF000);
        for (size_t i { 1 }; i < registers.x.size(); ++i) {
            EXPECT_EQ(registers.x[i], 0);
        }
        EXPECT_EQ(registers.spEl1, 0);
        throw GuestStopped {};
    };
    EXPECT_EQ(vmm.GetState(), VmState::READY);
    EXPECT_THROW(vmm.Run(), GuestStopped);
    EXPECT_EQ(vmm.GetState(), VmState::RUNNING);
}

TEST_F(VmTest, HandlesExitBeforeResumingGuest) {
    Vmm vmm { config(), MmioMap {} };
    size_t runs {};
    vcpuTest::run = [&](Vcpu& vcpu) -> VcpuExit {
        auto& registers { vcpu.GetRegisters() };
        if (runs++ == 0) {
            registers.x[0] = 0x84000000;
            registers.pc = 0x200004;
            return { ExitReason::SYNC, static_cast<uint64_t>(EsrEc::HVC_AARCH64) << 26 };
        }
        EXPECT_EQ(registers.x[0], 0x10000);
        EXPECT_EQ(registers.pc, 0x200004);
        registers.x[0] = 0x84000008;
        return { ExitReason::SYNC, static_cast<uint64_t>(EsrEc::HVC_AARCH64) << 26 };
    };
    EXPECT_EQ(vmm.Run().value(), VmState::SHUTDOWN);
    EXPECT_EQ(runs, 2);
}
