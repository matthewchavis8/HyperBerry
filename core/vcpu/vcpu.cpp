// @file vcpu.cpp
// @brief Per-guest vCPU context implementation.
// @ingroup vcpu

#include "vcpu.h"
#include "lib/panic/panic.h"

// EL1h, DAIF all masked: guest wakes with interrupts disabled
static constexpr uint64_t SPSR_EL1H_ALL_MASKED { (0b00101ULL) | (0xFULL << 6) };

// Bits cleared from SCTLR_EL1 reset value before handing control to guest
static constexpr uint64_t SCTLR_M_BIT { (1ULL << 0) };
static constexpr uint64_t SCTLR_A_BIT { (1ULL << 1) };
static constexpr uint64_t SCTLR_C_BIT { (1ULL << 2) };
static constexpr uint64_t SCTLR_SA_BIT { (1ULL << 3) };
static constexpr uint64_t SCTLR_I_BIT { (1ULL << 12) };

extern "C" void vcpu_restore_el1_sysregs(Vcpu* vcpu) {
    if (vcpu == nullptr) HvPanic("[VCPU] nullptr passed to vcpu_restore_el1_sysregs");

    vcpu->RestoreEl1SysRegs();
}

extern "C" void vcpu_save_el1_sysregs(Vcpu* vcpu) {
    if (vcpu == nullptr) HvPanic("[VCPU] nullptr passed to vcpu_save_el1_sysregs");

    vcpu->SaveEl1SysRegs();
}

Vcpu::Vcpu(uint64_t entrypoint) {
    el2(El2Reg::ELR_EL2) = entrypoint;
    el2(El2Reg::SPSR_EL2) = SPSR_EL1H_ALL_MASKED;

    uint64_t sctlr;
    asm volatile("mrs %0, sctlr_el1" : "=r"(sctlr));
    sctlr &= ~(SCTLR_M_BIT | SCTLR_A_BIT | SCTLR_C_BIT | SCTLR_SA_BIT | SCTLR_I_BIT);
    el1(El1Reg::SCTLR_EL1) = sctlr;
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

void Vcpu::SetGuestSp(uint64_t sp) {
    el1(El1Reg::SP_EL1) = sp;
}

uint64_t Vcpu::GetGpReg(Gpr reg) const noexcept {
    if (reg == Gpr::SP_EL0) return m_spEl0;
    return m_gpr[static_cast<size_t>(reg)];
}

void Vcpu::SetGpReg(Gpr reg, uint64_t val) {
    if (reg == Gpr::SP_EL0) {
        m_spEl0 = val;
        return;
    }
    m_gpr[static_cast<size_t>(reg)] = val;
}

void Vcpu::SaveEl1SysRegs() {
    asm volatile("mrs %0, sctlr_el1" : "=r"(el1(El1Reg::SCTLR_EL1)));
    asm volatile("mrs %0, ttbr0_el1" : "=r"(el1(El1Reg::TTBR0_EL1)));
    asm volatile("mrs %0, ttbr1_el1" : "=r"(el1(El1Reg::TTBR1_EL1)));
    asm volatile("mrs %0, tcr_el1" : "=r"(el1(El1Reg::TCR_EL1)));
    asm volatile("mrs %0, mair_el1" : "=r"(el1(El1Reg::MAIR_EL1)));
    asm volatile("mrs %0, amair_el1" : "=r"(el1(El1Reg::AMAIR_EL1)));
    asm volatile("mrs %0, vbar_el1" : "=r"(el1(El1Reg::VBAR_EL1)));
    asm volatile("mrs %0, elr_el1" : "=r"(el1(El1Reg::ELR_EL1)));
    asm volatile("mrs %0, spsr_el1" : "=r"(el1(El1Reg::SPSR_EL1)));
    asm volatile("mrs %0, sp_el1" : "=r"(el1(El1Reg::SP_EL1)));
    asm volatile("mrs %0, esr_el1" : "=r"(el1(El1Reg::ESR_EL1)));
    asm volatile("mrs %0, far_el1" : "=r"(el1(El1Reg::FAR_EL1)));
    asm volatile("mrs %0, afsr0_el1" : "=r"(el1(El1Reg::AFSR0_EL1)));
    asm volatile("mrs %0, afsr1_el1" : "=r"(el1(El1Reg::AFSR1_EL1)));
    asm volatile("mrs %0, contextidr_el1" : "=r"(el1(El1Reg::CONTEXTIDR_EL1)));
    asm volatile("mrs %0, tpidr_el1" : "=r"(el1(El1Reg::TPIDR_EL1)));
    asm volatile("mrs %0, tpidr_el0" : "=r"(el1(El1Reg::TPIDR_EL0)));
    asm volatile("mrs %0, tpidrro_el0" : "=r"(el1(El1Reg::TPIDRRO_EL0)));
    asm volatile("mrs %0, cntkctl_el1" : "=r"(el1(El1Reg::CNTKCTL_EL1)));
    asm volatile("mrs %0, cpacr_el1" : "=r"(el1(El1Reg::CPACR_EL1)));
    asm volatile("mrs %0, par_el1" : "=r"(el1(El1Reg::PAR_EL1)));
    asm volatile("mrs %0, csselr_el1" : "=r"(el1(El1Reg::CSSELR_EL1)));
}

void Vcpu::RestoreEl1SysRegs() {
    asm volatile("msr ttbr0_el1, %0" ::"r"(el1(El1Reg::TTBR0_EL1)));
    asm volatile("msr ttbr1_el1, %0" ::"r"(el1(El1Reg::TTBR1_EL1)));
    asm volatile("msr tcr_el1, %0" ::"r"(el1(El1Reg::TCR_EL1)));
    asm volatile("msr mair_el1, %0" ::"r"(el1(El1Reg::MAIR_EL1)));
    asm volatile("msr amair_el1, %0" ::"r"(el1(El1Reg::AMAIR_EL1)));
    asm volatile("msr vbar_el1, %0" ::"r"(el1(El1Reg::VBAR_EL1)));
    asm volatile("msr elr_el1, %0" ::"r"(el1(El1Reg::ELR_EL1)));
    asm volatile("msr spsr_el1, %0" ::"r"(el1(El1Reg::SPSR_EL1)));
    asm volatile("msr sp_el1, %0" ::"r"(el1(El1Reg::SP_EL1)));
    asm volatile("msr esr_el1, %0" ::"r"(el1(El1Reg::ESR_EL1)));
    asm volatile("msr far_el1, %0" ::"r"(el1(El1Reg::FAR_EL1)));
    asm volatile("msr afsr0_el1, %0" ::"r"(el1(El1Reg::AFSR0_EL1)));
    asm volatile("msr afsr1_el1, %0" ::"r"(el1(El1Reg::AFSR1_EL1)));
    asm volatile("msr contextidr_el1, %0" ::"r"(el1(El1Reg::CONTEXTIDR_EL1)));
    asm volatile("msr tpidr_el1, %0" ::"r"(el1(El1Reg::TPIDR_EL1)));
    asm volatile("msr tpidr_el0, %0" ::"r"(el1(El1Reg::TPIDR_EL0)));
    asm volatile("msr tpidrro_el0, %0" ::"r"(el1(El1Reg::TPIDRRO_EL0)));
    asm volatile("msr cntkctl_el1, %0" ::"r"(el1(El1Reg::CNTKCTL_EL1)));
    asm volatile("msr cpacr_el1, %0" ::"r"(el1(El1Reg::CPACR_EL1)));
    asm volatile("msr par_el1, %0" ::"r"(el1(El1Reg::PAR_EL1)));
    asm volatile("msr csselr_el1, %0" ::"r"(el1(El1Reg::CSSELR_EL1)));

    asm volatile("isb");
    asm volatile("msr sctlr_el1, %0" ::"r"(el1(El1Reg::SCTLR_EL1)));
    asm volatile("isb");
}

Vcpu* Vcpu::GetCurrentVcpu() {
    uint64_t currentVcpu;
    asm volatile("mrs %0, tpidr_el2" : "=r"(currentVcpu));
    return reinterpret_cast<Vcpu*>(static_cast<uintptr_t>(currentVcpu));
}

void Vcpu::ScheduleNext() {
    // TODO: replace with a real scheduler when the vCPU pool lands.
}
