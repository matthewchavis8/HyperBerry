// @file test_vcpu.cpp
// @brief Test the production Vcpu with only hardware entry replaced.

#include <gtest/gtest.h>
#include <type_traits>
#include "backend.h"

namespace vcpuTest {
std::function<VcpuExit(Vcpu&)> run;
}

extern "C" uint64_t vcpu_read_sctlr() {
    return 0x30D00800;
}

extern "C" void vcpu_run(Vcpu* vcpu, VcpuExit* exit) {
    *exit = vcpuTest::run(*vcpu);
}

static_assert(!std::is_copy_constructible_v<Vcpu>);
static_assert(!std::is_move_constructible_v<Vcpu>);

TEST(Vcpu, StartsAtEntryInMaskedEl1h) {
    const Vcpu vcpu { 0x40000000 };
    const auto& registers { vcpu.GetRegisters() };
    EXPECT_EQ(registers.pc, 0x40000000);
    EXPECT_EQ(registers.pstate, 0x3C5);
    EXPECT_EQ(registers.spEl0, 0);
    EXPECT_EQ(registers.spEl1, 0);
    for (const auto value : registers.x) {
        EXPECT_EQ(value, 0);
    }
}

TEST(Vcpu, RunExposesSavedStateAndReturnsExit) {
    Vcpu vcpu { 0x40000000 };
    vcpu.GetRegisters().x[0] = 123;
    vcpuTest::run = [&](Vcpu& running) {
        EXPECT_EQ(&running, &vcpu);
        EXPECT_EQ(running.GetRegisters().x[0], 123);
        running.GetRegisters().x[0] = 456;
        running.GetRegisters().pc += 4;
        return VcpuExit { ExitReason::SYNC, 0x58000042 };
    };
    const VcpuExit exit { vcpu.Run() };
    EXPECT_EQ(exit.reason, ExitReason::SYNC);
    EXPECT_EQ(exit.syndrome, 0x58000042);
    EXPECT_EQ(vcpu.GetRegisters().x[0], 456);
    EXPECT_EQ(vcpu.GetRegisters().pc, 0x40000004);
    vcpuTest::run = {};
}
