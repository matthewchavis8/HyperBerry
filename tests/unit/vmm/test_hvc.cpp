#include <gtest/gtest.h>
#include "core/vmm/hvc/hvc.h"

TEST(Hvc, VersionAndFeatures) {
    GuestRegisters registers {};
    registers.x[0] = 0x84000000;
    EXPECT_EQ(Hvc::Handle(registers, 0), Hvc::Action::RESUME);
    EXPECT_EQ(registers.x[0], 0x10000);
    registers.x[0] = 0x8400000A;
    registers.x[1] = 0x84000008;
    EXPECT_EQ(Hvc::Handle(registers, 0), Hvc::Action::RESUME);
    EXPECT_EQ(registers.x[0], 0);
    registers.x[0] = 0x8400000A;
    registers.x[1] = 0x84000001;
    EXPECT_EQ(Hvc::Handle(registers, 0), Hvc::Action::RESUME);
    EXPECT_EQ(registers.x[0], UINT64_MAX);
}

TEST(Hvc, PowerCallsReportAction) {
    GuestRegisters registers {};
    registers.x[0] = 0x84000008;
    EXPECT_EQ(Hvc::Handle(registers, 0), Hvc::Action::SHUTDOWN);
    registers.x[0] = 0x84000009;
    EXPECT_EQ(Hvc::Handle(registers, 0), Hvc::Action::RESET);
}

TEST(Hvc, UnsupportedOwnerCallAndImmediateReturnNotSupported) {
    GuestRegisters registers {};
    registers.x[0] = 0xDEADBEEF;
    EXPECT_EQ(Hvc::Handle(registers, 0), Hvc::Action::RESUME);
    EXPECT_EQ(registers.x[0], UINT64_MAX);
    registers.x[0] = 0x84000000;
    EXPECT_EQ(Hvc::Handle(registers, 1), Hvc::Action::RESUME);
    EXPECT_EQ(registers.x[0], UINT64_MAX);
}
