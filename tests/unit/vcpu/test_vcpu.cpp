/**
 * @file test_vcpu.cpp
 * @brief Hosted unit tests for Vcpu layout and basic behavior.
 */

#include <gtest/gtest.h>

#include "core/vcpu/vcpu.h"
#include "lib/strings/strings.h"

static constexpr uint64_t SPSR_EL1H_ALL_MASKED =
    (0b00101ULL) |
    (0xFULL << 6);

void Vcpu::init(uint64_t entrypoint) {
  memset(this, 0, sizeof(*this));

  m_el2State.regs[VCPU_ELR_EL2 / sizeof(uint64_t)] = entrypoint;
  m_el2State.regs[VCPU_SPSR_EL2 / sizeof(uint64_t)] = SPSR_EL1H_ALL_MASKED;
  m_el1SysRegs.regs[VCPU_SCTLR_EL1 / sizeof(uint64_t)] = 0;
}

uint64_t Vcpu::elr() const {
  return m_el2State.regs[VCPU_ELR_EL2 / sizeof(uint64_t)];
}

void Vcpu::setPc(uint64_t pc) {
  m_el2State.regs[VCPU_ELR_EL2 / sizeof(uint64_t)] = pc;
}

void Vcpu::skipInstruction() {
  setPc(elr() + 4);
}

uint64_t Vcpu::gpReg(uint64_t off) const {
  return m_gpRegs.regs[off / sizeof(uint64_t)];
}

void Vcpu::setGpReg(uint64_t off, uint64_t val) {
  m_gpRegs.regs[off / sizeof(uint64_t)] = val;
}

void Vcpu::saveEl1SysRegs() {}

void Vcpu::restoreEl1SysRegs() {}

Vcpu* Vcpu::current() {
  return nullptr;
}

void Vcpu::scheduleNext() {}

TEST(Vcpu, IsStandardLayout) {
  EXPECT_TRUE(__is_standard_layout(Vcpu));
}

TEST(Vcpu, SubStructOffsetsMatchAsmContract) {
  EXPECT_EQ(VcpuLayoutAccess::gpRegsOffset(), VCPU_GPREGS_OFFSET);
  EXPECT_EQ(VcpuLayoutAccess::el2StateOffset(), VCPU_EL2STATE_OFFSET);
  EXPECT_EQ(VcpuLayoutAccess::el1SysRegsOffset(), VCPU_EL1REGS_OFFSET);
  EXPECT_GE(sizeof(Vcpu), VCPU_SIZEOF);
}

TEST(Vcpu, InitSeedsEntrypointInElr) {
  Vcpu vcpu;
  vcpu.init(0x40000000ULL);

  EXPECT_EQ(vcpu.elr(), 0x40000000ULL);
}

TEST(Vcpu, InitZeroesGpRegs) {
  Vcpu vcpu;
  vcpu.setGpReg(VCPU_GPREG_X5, 0xDEADBEEFULL);

  vcpu.init(0x40000000ULL);

  EXPECT_EQ(vcpu.gpReg(VCPU_GPREG_X5), 0ULL);
}

TEST(Vcpu, CurrentReturnsNullOnHostedBuilds) {
  EXPECT_EQ(Vcpu::current(), nullptr);
}
