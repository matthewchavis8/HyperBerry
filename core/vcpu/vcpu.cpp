// @file vcpu.cpp
// @brief Guest initialization and the assembly execution boundary.

#include "vcpu.h"

extern "C" uint64_t vcpu_read_sctlr();
extern "C" VcpuExit vcpu_run(Vcpu* vcpu);

Vcpu::Vcpu(uint64_t entry) : m_registers { .pc = entry } {
    constexpr uint64_t DISABLED_CONTROLS { (1ULL << 0) | (1ULL << 1) |
            (1ULL << 2) | (1ULL << 3) | (1ULL << 12) };
    m_system.sctlr = vcpu_read_sctlr() & ~DISABLED_CONTROLS;
}

VcpuExit Vcpu::Run() {
    return vcpu_run(this);
}

GuestRegisters& Vcpu::GetRegisters() noexcept {
    return m_registers;
}

const GuestRegisters& Vcpu::GetRegisters() const noexcept {
    return m_registers;
}
