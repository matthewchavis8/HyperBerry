/**
 * @file vcpu.cpp
 * @brief Per-guest vCPU context implementation.
 * @ingroup vcpu
 */

#include "vcpu.h"
#include "drivers/uart/uart.h"
#include "lib/strings/strings.h"

// EL1h, DAIF all masked: guest wakes with interrupts disabled
static constexpr uint64_t SPSR_EL1H_ALL_MASKED =
    (0b00101ULL) |
    (0xFULL << 6);

// Bits cleared from SCTLR_EL1 reset value before handing control to guest
static constexpr uint64_t SCTLR_M_BIT  = (1ULL << 0);
static constexpr uint64_t SCTLR_A_BIT  = (1ULL << 1);
static constexpr uint64_t SCTLR_C_BIT  = (1ULL << 2);
static constexpr uint64_t SCTLR_SA_BIT = (1ULL << 3);
static constexpr uint64_t SCTLR_I_BIT  = (1ULL << 12);

extern "C" void vcpu_restore_el1_sysregs(Vcpu* vcpu) {
  if (vcpu == nullptr) {
    Uart::println("[ERROR] nullptr passed to vcpu_restore_el1_sysregs");
    for (;;) asm volatile("wfe"); // TODO: Add a generic panic handler soon
  }
  
  vcpu->restoreEl1SysRegs();
}

extern "C" void vcpu_save_el1_sysregs(Vcpu* vcpu) {
  if (vcpu == nullptr) {
    Uart::println("[ERROR] nullptr passed to vcpu_restore_el1_sysregs");
    for (;;) asm volatile("wfe"); // TODO: Add a generic panic handler soon
  }
  
  vcpu->saveEl1SysRegs();
}

void Vcpu::init(uint64_t entrypoint) {
  memset(this, 0, sizeof(*this));

  m_el2State.regs[regIdx(VCPU_ELR_EL2)] = entrypoint;
  m_el2State.regs[regIdx(VCPU_SPSR_EL2)] = SPSR_EL1H_ALL_MASKED;

  uint64_t sctlr;
  asm volatile("mrs %0, sctlr_el1" : "=r"(sctlr));
  sctlr &= ~(SCTLR_M_BIT | SCTLR_A_BIT | SCTLR_C_BIT | SCTLR_SA_BIT | SCTLR_I_BIT);
  m_el1SysRegs.regs[regIdx(VCPU_SCTLR_EL1)] = sctlr;
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

void Vcpu::setGuestSp(uint64_t sp) {
  m_el1SysRegs.regs[regIdx(VCPU_SP_EL1)] = sp;
}

uint64_t Vcpu::getGpReg(uint64_t off) const {
  return m_gpRegs.regs[regIdx(off)];
}

void Vcpu::setGpReg(uint64_t off, uint64_t val) {
  m_gpRegs.regs[regIdx(off)] = val;
}

#define VCPU_SAVE_EL1(name, off) do {                                     \
    uint64_t value;                                                       \
    asm volatile("mrs %0, " #name : "=r"(value));                         \
    m_el1SysRegs.regs[regIdx(off)] = value;                               \
  } while (0)

#define VCPU_RESTORE_EL1(name, off) do {                                  \
    uint64_t value = m_el1SysRegs.regs[regIdx(off)];                      \
    asm volatile("msr " #name ", %0" :: "r"(value));                      \
  } while (0)

void Vcpu::saveEl1SysRegs() {
  VCPU_SAVE_EL1(sctlr_el1,      VCPU_SCTLR_EL1);
  VCPU_SAVE_EL1(ttbr0_el1,      VCPU_TTBR0_EL1);
  VCPU_SAVE_EL1(ttbr1_el1,      VCPU_TTBR1_EL1);
  VCPU_SAVE_EL1(tcr_el1,        VCPU_TCR_EL1);
  VCPU_SAVE_EL1(mair_el1,       VCPU_MAIR_EL1);
  VCPU_SAVE_EL1(amair_el1,      VCPU_AMAIR_EL1);
  VCPU_SAVE_EL1(vbar_el1,       VCPU_VBAR_EL1);
  VCPU_SAVE_EL1(elr_el1,        VCPU_ELR_EL1);
  VCPU_SAVE_EL1(spsr_el1,       VCPU_SPSR_EL1);
  VCPU_SAVE_EL1(sp_el1,         VCPU_SP_EL1);
  VCPU_SAVE_EL1(esr_el1,        VCPU_ESR_EL1);
  VCPU_SAVE_EL1(far_el1,        VCPU_FAR_EL1);
  VCPU_SAVE_EL1(afsr0_el1,      VCPU_AFSR0_EL1);
  VCPU_SAVE_EL1(afsr1_el1,      VCPU_AFSR1_EL1);
  VCPU_SAVE_EL1(contextidr_el1, VCPU_CONTEXTIDR_EL1);
  VCPU_SAVE_EL1(tpidr_el1,      VCPU_TPIDR_EL1);
  VCPU_SAVE_EL1(tpidr_el0,      VCPU_TPIDR_EL0);
  VCPU_SAVE_EL1(tpidrro_el0,    VCPU_TPIDRRO_EL0);
  VCPU_SAVE_EL1(cntkctl_el1,    VCPU_CNTKCTL_EL1);
  VCPU_SAVE_EL1(cpacr_el1,      VCPU_CPACR_EL1);
  VCPU_SAVE_EL1(par_el1,        VCPU_PAR_EL1);
  VCPU_SAVE_EL1(csselr_el1,     VCPU_CSSELR_EL1);
}

void Vcpu::restoreEl1SysRegs() {
  VCPU_RESTORE_EL1(ttbr0_el1,      VCPU_TTBR0_EL1);
  VCPU_RESTORE_EL1(ttbr1_el1,      VCPU_TTBR1_EL1);
  VCPU_RESTORE_EL1(tcr_el1,        VCPU_TCR_EL1);
  VCPU_RESTORE_EL1(mair_el1,       VCPU_MAIR_EL1);
  VCPU_RESTORE_EL1(amair_el1,      VCPU_AMAIR_EL1);
  VCPU_RESTORE_EL1(vbar_el1,       VCPU_VBAR_EL1);
  VCPU_RESTORE_EL1(elr_el1,        VCPU_ELR_EL1);
  VCPU_RESTORE_EL1(spsr_el1,       VCPU_SPSR_EL1);
  VCPU_RESTORE_EL1(sp_el1,         VCPU_SP_EL1);
  VCPU_RESTORE_EL1(esr_el1,        VCPU_ESR_EL1);
  VCPU_RESTORE_EL1(far_el1,        VCPU_FAR_EL1);
  VCPU_RESTORE_EL1(afsr0_el1,      VCPU_AFSR0_EL1);
  VCPU_RESTORE_EL1(afsr1_el1,      VCPU_AFSR1_EL1);
  VCPU_RESTORE_EL1(contextidr_el1, VCPU_CONTEXTIDR_EL1);
  VCPU_RESTORE_EL1(tpidr_el1,      VCPU_TPIDR_EL1);
  VCPU_RESTORE_EL1(tpidr_el0,      VCPU_TPIDR_EL0);
  VCPU_RESTORE_EL1(tpidrro_el0,    VCPU_TPIDRRO_EL0);
  VCPU_RESTORE_EL1(cntkctl_el1,    VCPU_CNTKCTL_EL1);
  VCPU_RESTORE_EL1(cpacr_el1,      VCPU_CPACR_EL1);
  VCPU_RESTORE_EL1(par_el1,        VCPU_PAR_EL1);
  VCPU_RESTORE_EL1(csselr_el1,     VCPU_CSSELR_EL1);

  asm volatile("isb");
  VCPU_RESTORE_EL1(sctlr_el1, VCPU_SCTLR_EL1);
  asm volatile("isb");
}

Vcpu* Vcpu::getCurrentVcpu() {
  uint64_t currentVcpu;
  asm volatile("mrs %0, tpidr_el2" : "=r"(currentVcpu));
  return reinterpret_cast<Vcpu*>(static_cast<uintptr_t>(currentVcpu));
}

void Vcpu::scheduleNext() {
  // TODO: replace with a real scheduler when the vCPU pool lands.
}
