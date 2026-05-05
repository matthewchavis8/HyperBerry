/**
 * @file test_vcpu.cpp
 * @brief Hosted unit tests for Vcpu layout and basic behavior.
 */

#include <gtest/gtest.h>

#include "core/vcpu/vcpu.h"

static constexpr uint64_t SPSR_EL1H_ALL_MASKED =
    (0b00101ULL) |
    (0xFULL << 6);

// Capture globals — readable from other translation units (e.g. test_vm.cpp)
// via extern declarations. Sentinel 0xDEADDEADDEADDEADULL means "not set".
uint64_t gVcpuInitEntryCap   = 0xDEADDEADDEADDEADULL;
uint64_t gVcpuSetGuestSpCap  = 0xDEADDEADDEADDEADULL;
uint64_t gVcpuSetX0Cap       = 0xDEADDEADDEADDEADULL;

void Vcpu::init(uint64_t entrypoint) {
  gVcpuInitEntryCap = entrypoint;
  memset(this, 0, sizeof(*this));

  m_el2State.regs[regIdx(VCPU_ELR_EL2)] = entrypoint;
  m_el2State.regs[regIdx(VCPU_SPSR_EL2)] = SPSR_EL1H_ALL_MASKED;
  m_el1SysRegs.regs[regIdx(VCPU_SCTLR_EL1)] = 0;
}

uint64_t Vcpu::getElr() const {
  return m_el2State.regs[regIdx(VCPU_ELR_EL2)];
}

void Vcpu::setPc(uint64_t pc) {
  m_el2State.regs[regIdx(VCPU_ELR_EL2)] = pc;
}

void Vcpu::skipInstruction() {
  setPc(getElr() + 4);
}

uint64_t Vcpu::getGpReg(uint64_t off) const {
  if (off == VCPU_GPREG_SP_EL0)
    return m_spEl0;
  return m_gpr[regIdx(off)];
}

void Vcpu::setGpReg(uint64_t off, uint64_t val) {
  if (off == VCPU_GPREG_X0)
    gVcpuSetX0Cap = val;
  if (off == VCPU_GPREG_SP_EL0) {
    m_spEl0 = val;
    return;
  }
  m_gpr[regIdx(off)] = val;
}

void Vcpu::saveEl1SysRegs() {}

void Vcpu::restoreEl1SysRegs() {}

Vcpu* Vcpu::getCurrentVcpu() {
  return nullptr;
}

void Vcpu::scheduleNext() {}

void Vcpu::setGuestSp(uint64_t sp) {
  gVcpuSetGuestSpCap = sp;
  m_el1SysRegs.regs[regIdx(VCPU_SP_EL1)] = sp;
}

TEST(Vcpu, IsStandardLayout) {
  EXPECT_TRUE(__is_standard_layout(Vcpu));
}

TEST(Vcpu, SubStructOffsetsMatchAsmContract) {
  EXPECT_EQ(VcpuLayoutAccess::gprOffset(), VCPU_GPREGS_OFFSET);
  EXPECT_EQ(VcpuLayoutAccess::spEl0Offset(), VCPU_GPREG_SP_EL0);
  EXPECT_EQ(VcpuLayoutAccess::el2StateOffset(), VCPU_EL2STATE_OFFSET);
  EXPECT_EQ(VcpuLayoutAccess::el1SysRegsOffset(), VCPU_EL1REGS_OFFSET);
  EXPECT_GE(sizeof(Vcpu), VCPU_SIZEOF);
}

TEST(Vcpu, InitSeedsEntrypointInElr) {
  Vcpu vcpu;
  vcpu.init(0x40000000ULL);

  EXPECT_EQ(vcpu.getElr(), 0x40000000ULL);
}

TEST(Vcpu, InitZeroesGprs) {
  Vcpu vcpu;
  vcpu.setGpReg(VCPU_GPREG_X5, 0xDEADBEEFULL);

  vcpu.init(0x40000000ULL);

  EXPECT_EQ(vcpu.getGpReg(VCPU_GPREG_X5), 0ULL);
}

TEST(Vcpu, CurrentReturnsNullOnHostedBuilds) {
  EXPECT_EQ(Vcpu::getCurrentVcpu(), nullptr);
}

TEST(Vcpu, SkipInstructionAdvancesElrByFour) {
  Vcpu vcpu;
  vcpu.init(0x40001000ULL);
  vcpu.skipInstruction();
  EXPECT_EQ(vcpu.getElr(), 0x40001004ULL);
}

TEST(Vcpu, SetPcRoundTrips) {
  Vcpu vcpu;
  vcpu.init(0x40000000ULL);
  vcpu.setPc(0xDEAD0000ULL);
  EXPECT_EQ(vcpu.getElr(), 0xDEAD0000ULL);
}

TEST(Vcpu, SetGpRegRoundTripsAcrossMultipleOffsets) {
  Vcpu vcpu;
  vcpu.init(0x40000000ULL);
  vcpu.setGpReg(VCPU_GPREG_X0,  0x1111111111111111ULL);
  vcpu.setGpReg(VCPU_GPREG_X15, 0x2222222222222222ULL);
  vcpu.setGpReg(VCPU_GPREG_X29, 0x3333333333333333ULL);
  vcpu.setGpReg(VCPU_GPREG_LR,  0x4444444444444444ULL);
  EXPECT_EQ(vcpu.getGpReg(VCPU_GPREG_X0),  0x1111111111111111ULL);
  EXPECT_EQ(vcpu.getGpReg(VCPU_GPREG_X15), 0x2222222222222222ULL);
  EXPECT_EQ(vcpu.getGpReg(VCPU_GPREG_X29), 0x3333333333333333ULL);
  EXPECT_EQ(vcpu.getGpReg(VCPU_GPREG_LR),  0x4444444444444444ULL);
}

TEST(Vcpu, HvContextSizeMatchesAsmConstant) {
  EXPECT_EQ(sizeof(HvContext), (size_t)VCPU_HVCTX_SIZE);
}

TEST(Vcpu, HvContextOffsetMatchesAsmConstant) {
  EXPECT_EQ(VcpuLayoutAccess::hvCtxOffset(), (uint64_t)VCPU_HVCTX_OFFSET);
}

TEST(Vcpu, SetIdGetIdRoundTrip) {
  Vcpu vcpu;
  vcpu.init(0x40000000ULL);
  vcpu.setId(42);
  EXPECT_EQ(vcpu.getId(), 42U);
}
