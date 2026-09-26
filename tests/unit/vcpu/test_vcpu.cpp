// @file test_vcpu.cpp
// @brief Hosted unit tests for Vcpu layout and basic behavior.

#include <gtest/gtest.h>

#include "core/vcpu/vcpu.h"

static constexpr uint64_t SPSR_EL1H_ALL_MASKED { (0b00101ULL) | (0xFULL << 6) };

// Capture globals — readable from other translation units (e.g. test_vm.cpp)
// via extern declarations. Sentinel 0xDEADDEADDEADDEADULL means "not set".
uint64_t gVcpuEntryCap { 0xDEADDEADDEADDEADULL };
uint64_t gVcpuSetGuestSpCap { 0xDEADDEADDEADDEADULL };
uint64_t gVcpuSetX0Cap { 0xDEADDEADDEADDEADULL };

Vcpu::Vcpu(uint64_t entrypoint) {
    gVcpuEntryCap = entrypoint;
    el2(El2Reg::ELR_EL2) = entrypoint;
    el2(El2Reg::SPSR_EL2) = SPSR_EL1H_ALL_MASKED;
}

uint64_t Vcpu::GetElr() const noexcept {
    return el2(El2Reg::ELR_EL2);
}

void Vcpu::SetPc(uint64_t pc) {
    el2(El2Reg::ELR_EL2) = pc;
}

void Vcpu::SkipInstruction() {
    SetPc(GetElr() + 4);
}

uint64_t Vcpu::GetGpReg(Gpr reg) const noexcept {
    if (reg == Gpr::SP_EL0) return m_spEl0;
    return m_gpr[static_cast<size_t>(reg)];
}

void Vcpu::SetGpReg(Gpr reg, uint64_t val) {
    if (reg == Gpr::X0) gVcpuSetX0Cap = val;
    if (reg == Gpr::SP_EL0) {
        m_spEl0 = val;
        return;
    }
    m_gpr[static_cast<size_t>(reg)] = val;
}

void Vcpu::SaveEl1SysRegs() {}

void Vcpu::RestoreEl1SysRegs() {}

Vcpu* Vcpu::GetCurrentVcpu() {
    return nullptr;
}

void Vcpu::ScheduleNext() {}

void Vcpu::SetGuestSp(uint64_t sp) {
    gVcpuSetGuestSpCap = sp;
    el1(El1Reg::SP_EL1) = sp;
}

TEST(Vcpu, IsStandardLayout) {
    EXPECT_TRUE(__is_standard_layout(Vcpu));
}

TEST(Vcpu, ConstructorSeedsEntrypointInElr) {
    Vcpu vcpu { 0x40000000ULL };

    EXPECT_EQ(vcpu.GetElr(), 0x40000000ULL);
}

TEST(Vcpu, ConstructorZeroesGprs) {
    Vcpu vcpu { 0x40000000ULL };

    EXPECT_EQ(vcpu.GetGpReg(Gpr::X5), 0ULL);
}

TEST(Vcpu, CurrentReturnsNullOnHostedBuilds) {
    EXPECT_EQ(Vcpu::GetCurrentVcpu(), nullptr);
}

TEST(Vcpu, SkipInstructionAdvancesElrByFour) {
    Vcpu vcpu { 0x40001000ULL };
    vcpu.SkipInstruction();
    EXPECT_EQ(vcpu.GetElr(), 0x40001004ULL);
}

TEST(Vcpu, SetPcRoundTrips) {
    Vcpu vcpu { 0x40000000ULL };
    vcpu.SetPc(0xDEAD0000ULL);
    EXPECT_EQ(vcpu.GetElr(), 0xDEAD0000ULL);
}

TEST(Vcpu, SetGpRegRoundTripsAcrossRegisters) {
    Vcpu vcpu { 0x40000000ULL };
    vcpu.SetGpReg(Gpr::X0, 0x1111111111111111ULL);
    vcpu.SetGpReg(Gpr::X15, 0x2222222222222222ULL);
    vcpu.SetGpReg(Gpr::X29, 0x3333333333333333ULL);
    vcpu.SetGpReg(Gpr::LR, 0x4444444444444444ULL);
    EXPECT_EQ(vcpu.GetGpReg(Gpr::X0), 0x1111111111111111ULL);
    EXPECT_EQ(vcpu.GetGpReg(Gpr::X15), 0x2222222222222222ULL);
    EXPECT_EQ(vcpu.GetGpReg(Gpr::X29), 0x3333333333333333ULL);
    EXPECT_EQ(vcpu.GetGpReg(Gpr::LR), 0x4444444444444444ULL);
}

TEST(Vcpu, SpEl0IsSeparateFromGprs) {
    Vcpu vcpu { 0x40000000ULL };
    vcpu.SetGpReg(Gpr::SP_EL0, 0x5555555555555555ULL);
    EXPECT_EQ(vcpu.GetGpReg(Gpr::SP_EL0), 0x5555555555555555ULL);
    EXPECT_EQ(vcpu.GetGprs()[30], 0ULL);
}

TEST(Vcpu, SetIdGetIdRoundTrip) {
    Vcpu vcpu { 0x40000000ULL };
    vcpu.SetId(42);
    EXPECT_EQ(vcpu.GetId(), 42U);
}
