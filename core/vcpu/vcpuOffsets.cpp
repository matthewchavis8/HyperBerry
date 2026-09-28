// @file vcpuOffsets.cpp
// @brief Emit compiler chosen offsets for the assembly guest context.

#include "vcpu.h"
#include <cstddef>
#include <type_traits>

#define OFFSET(symbol, value) asm volatile("\n.ascii \"->" #symbol " %c0\"" ::"i"(value))

struct VcpuLayout {
    static void Emit() {
        static_assert(std::is_standard_layout_v<Vcpu>);
        static_assert(std::is_standard_layout_v<GuestRegisters>);
        static_assert(std::is_standard_layout_v<VcpuExit>);
        static_assert(sizeof(VcpuExit) == 16);
        static_assert(offsetof(VcpuExit, reason) == 0);
        static_assert(offsetof(VcpuExit, syndrome) == 8);

        OFFSET(VCPU_X0, offsetof(Vcpu, m_registers) + offsetof(GuestRegisters, x) + 0 * sizeof(uint64_t));
        OFFSET(VCPU_X1, offsetof(Vcpu, m_registers) + offsetof(GuestRegisters, x) + 1 * sizeof(uint64_t));
        OFFSET(VCPU_X2, offsetof(Vcpu, m_registers) + offsetof(GuestRegisters, x) + 2 * sizeof(uint64_t));
        OFFSET(VCPU_X3, offsetof(Vcpu, m_registers) + offsetof(GuestRegisters, x) + 3 * sizeof(uint64_t));
        OFFSET(VCPU_X4, offsetof(Vcpu, m_registers) + offsetof(GuestRegisters, x) + 4 * sizeof(uint64_t));
        OFFSET(VCPU_X5, offsetof(Vcpu, m_registers) + offsetof(GuestRegisters, x) + 5 * sizeof(uint64_t));
        OFFSET(VCPU_X6, offsetof(Vcpu, m_registers) + offsetof(GuestRegisters, x) + 6 * sizeof(uint64_t));
        OFFSET(VCPU_X7, offsetof(Vcpu, m_registers) + offsetof(GuestRegisters, x) + 7 * sizeof(uint64_t));
        OFFSET(VCPU_X8, offsetof(Vcpu, m_registers) + offsetof(GuestRegisters, x) + 8 * sizeof(uint64_t));
        OFFSET(VCPU_X9, offsetof(Vcpu, m_registers) + offsetof(GuestRegisters, x) + 9 * sizeof(uint64_t));
        OFFSET(VCPU_X10, offsetof(Vcpu, m_registers) + offsetof(GuestRegisters, x) + 10 * sizeof(uint64_t));
        OFFSET(VCPU_X11, offsetof(Vcpu, m_registers) + offsetof(GuestRegisters, x) + 11 * sizeof(uint64_t));
        OFFSET(VCPU_X12, offsetof(Vcpu, m_registers) + offsetof(GuestRegisters, x) + 12 * sizeof(uint64_t));
        OFFSET(VCPU_X13, offsetof(Vcpu, m_registers) + offsetof(GuestRegisters, x) + 13 * sizeof(uint64_t));
        OFFSET(VCPU_X14, offsetof(Vcpu, m_registers) + offsetof(GuestRegisters, x) + 14 * sizeof(uint64_t));
        OFFSET(VCPU_X15, offsetof(Vcpu, m_registers) + offsetof(GuestRegisters, x) + 15 * sizeof(uint64_t));
        OFFSET(VCPU_X16, offsetof(Vcpu, m_registers) + offsetof(GuestRegisters, x) + 16 * sizeof(uint64_t));
        OFFSET(VCPU_X17, offsetof(Vcpu, m_registers) + offsetof(GuestRegisters, x) + 17 * sizeof(uint64_t));
        OFFSET(VCPU_X18, offsetof(Vcpu, m_registers) + offsetof(GuestRegisters, x) + 18 * sizeof(uint64_t));
        OFFSET(VCPU_X19, offsetof(Vcpu, m_registers) + offsetof(GuestRegisters, x) + 19 * sizeof(uint64_t));
        OFFSET(VCPU_X20, offsetof(Vcpu, m_registers) + offsetof(GuestRegisters, x) + 20 * sizeof(uint64_t));
        OFFSET(VCPU_X21, offsetof(Vcpu, m_registers) + offsetof(GuestRegisters, x) + 21 * sizeof(uint64_t));
        OFFSET(VCPU_X22, offsetof(Vcpu, m_registers) + offsetof(GuestRegisters, x) + 22 * sizeof(uint64_t));
        OFFSET(VCPU_X23, offsetof(Vcpu, m_registers) + offsetof(GuestRegisters, x) + 23 * sizeof(uint64_t));
        OFFSET(VCPU_X24, offsetof(Vcpu, m_registers) + offsetof(GuestRegisters, x) + 24 * sizeof(uint64_t));
        OFFSET(VCPU_X25, offsetof(Vcpu, m_registers) + offsetof(GuestRegisters, x) + 25 * sizeof(uint64_t));
        OFFSET(VCPU_X26, offsetof(Vcpu, m_registers) + offsetof(GuestRegisters, x) + 26 * sizeof(uint64_t));
        OFFSET(VCPU_X27, offsetof(Vcpu, m_registers) + offsetof(GuestRegisters, x) + 27 * sizeof(uint64_t));
        OFFSET(VCPU_X28, offsetof(Vcpu, m_registers) + offsetof(GuestRegisters, x) + 28 * sizeof(uint64_t));
        OFFSET(VCPU_X29, offsetof(Vcpu, m_registers) + offsetof(GuestRegisters, x) + 29 * sizeof(uint64_t));
        OFFSET(VCPU_X30, offsetof(Vcpu, m_registers) + offsetof(GuestRegisters, x) + 30 * sizeof(uint64_t));
        OFFSET(VCPU_PC, offsetof(Vcpu, m_registers) + offsetof(GuestRegisters, pc));
        OFFSET(VCPU_PSTATE, offsetof(Vcpu, m_registers) + offsetof(GuestRegisters, pstate));
        OFFSET(VCPU_SP_EL0, offsetof(Vcpu, m_registers) + offsetof(GuestRegisters, spEl0));
        OFFSET(VCPU_SP_EL1, offsetof(Vcpu, m_registers) + offsetof(GuestRegisters, spEl1));
        OFFSET(VCPU_SCTLR_EL1, offsetof(Vcpu, m_system) + offsetof(Vcpu::SystemRegisters, sctlr));
        OFFSET(VCPU_TTBR0_EL1, offsetof(Vcpu, m_system) + offsetof(Vcpu::SystemRegisters, ttbr0));
        OFFSET(VCPU_TTBR1_EL1, offsetof(Vcpu, m_system) + offsetof(Vcpu::SystemRegisters, ttbr1));
        OFFSET(VCPU_TCR_EL1, offsetof(Vcpu, m_system) + offsetof(Vcpu::SystemRegisters, tcr));
        OFFSET(VCPU_MAIR_EL1, offsetof(Vcpu, m_system) + offsetof(Vcpu::SystemRegisters, mair));
        OFFSET(VCPU_AMAIR_EL1, offsetof(Vcpu, m_system) + offsetof(Vcpu::SystemRegisters, amair));
        OFFSET(VCPU_VBAR_EL1, offsetof(Vcpu, m_system) + offsetof(Vcpu::SystemRegisters, vbar));
        OFFSET(VCPU_ELR_EL1, offsetof(Vcpu, m_system) + offsetof(Vcpu::SystemRegisters, elr));
        OFFSET(VCPU_SPSR_EL1, offsetof(Vcpu, m_system) + offsetof(Vcpu::SystemRegisters, spsr));
        OFFSET(VCPU_ESR_EL1, offsetof(Vcpu, m_system) + offsetof(Vcpu::SystemRegisters, esr));
        OFFSET(VCPU_FAR_EL1, offsetof(Vcpu, m_system) + offsetof(Vcpu::SystemRegisters, far));
        OFFSET(VCPU_AFSR0_EL1, offsetof(Vcpu, m_system) + offsetof(Vcpu::SystemRegisters, afsr0));
        OFFSET(VCPU_AFSR1_EL1, offsetof(Vcpu, m_system) + offsetof(Vcpu::SystemRegisters, afsr1));
        OFFSET(VCPU_CONTEXTIDR_EL1, offsetof(Vcpu, m_system) + offsetof(Vcpu::SystemRegisters, contextidr));
        OFFSET(VCPU_TPIDR_EL1, offsetof(Vcpu, m_system) + offsetof(Vcpu::SystemRegisters, tpidr));
        OFFSET(VCPU_TPIDR_EL0, offsetof(Vcpu, m_system) + offsetof(Vcpu::SystemRegisters, tpidrEl0));
        OFFSET(VCPU_TPIDRRO_EL0, offsetof(Vcpu, m_system) + offsetof(Vcpu::SystemRegisters, tpidrroEl0));
        OFFSET(VCPU_CNTKCTL_EL1, offsetof(Vcpu, m_system) + offsetof(Vcpu::SystemRegisters, cntkctl));
        OFFSET(VCPU_CPACR_EL1, offsetof(Vcpu, m_system) + offsetof(Vcpu::SystemRegisters, cpacr));
        OFFSET(VCPU_PAR_EL1, offsetof(Vcpu, m_system) + offsetof(Vcpu::SystemRegisters, par));
        OFFSET(VCPU_CSSELR_EL1, offsetof(Vcpu, m_system) + offsetof(Vcpu::SystemRegisters, csselr));
        OFFSET(VCPU_EXIT_SYNC, static_cast<uint64_t>(ExitReason::SYNC));
        OFFSET(VCPU_EXIT_IRQ, static_cast<uint64_t>(ExitReason::IRQ));
        OFFSET(VCPU_EXIT_FIQ, static_cast<uint64_t>(ExitReason::FIQ));
        OFFSET(VCPU_EXIT_SERROR, static_cast<uint64_t>(ExitReason::SERROR));
    }
};

extern "C" void vcpu_offsets() {
    VcpuLayout::Emit();
}
