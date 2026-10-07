// @file vm.cpp
// @brief Guest address space activation and execution loop.
// @ingroup vm

#include "lib/log/log.h"
#include "vm.h"

Vm::Vm(const VmConfig& config, const MmioMap& devices) :
            m_name { config.name },
            m_guestMmu { config.ipaBase, config.ramHostPa, config.ramSize, devices },
            m_vcpu { config.entry },
            m_vmid { config.vmid } {
    m_vcpu.GetRegisters().x[0] = config.dtb;
    Log::Println("[VM] {} built", m_name);
}

void Vm::Start() {
    Log::Println("[VM] Enabling Guest MMU");
    m_guestMmu.Enable(m_vmid);
    Log::Println("[VM] Successfully enabled Guest MMU");

    m_state = VmState::RUNNING;
}

VcpuExit Vm::Enter() {
    m_lastExit = m_vcpu.Run();
    return m_lastExit;
}

void Vm::Stop(VmState state) {
    m_state = state;
}

GuestRegisters& Vm::GetRegisters() noexcept {
    return m_vcpu.GetRegisters();
}

VmState Vm::GetState() const noexcept {
    return m_state;
}

const VcpuExit& Vm::GetLastExit() const noexcept {
    return m_lastExit;
}

[[nodiscard]] std::string_view Vm::GetName() const noexcept {
    return m_name;
}

[[nodiscard]] uint8_t Vm::GetVmId() const noexcept {
    return m_vmid;
}
