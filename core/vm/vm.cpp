// @file vm.cpp
// @brief Per-guest VM container implementation.
// @ingroup vm

#include "core/mm/pmm/pmm.h"
#include "lib/log/log.h"
#include "vm.h"

Vm::Vm(const VmConfig& config, const MmioMap& devices) :
            m_name { config.name },
            m_guestMmu { config.ipaBase, config.ramHostPa, config.ramSize, devices },
            m_vcpu { config.entry },
            m_vmid { config.vmid } {
    m_vcpu.SetGpReg(Gpr::X0, config.dtb);
    Log::Println("[VM] {} built", m_name);
}

void Vm::Run() {
    Log::Println("[VM] Enabling Guest MMU");
    m_guestMmu.Enable(m_vmid);
    Log::Println("[VM] Successfully enabled Guest MMU");

    Log::Println("[VM] Guest Kernel Running");
    vcpu_enter(&m_vcpu);
}

[[nodiscard]] std::string_view Vm::GetName() const noexcept {
    return m_name;
}

[[nodiscard]] uint8_t Vm::GetVmId() const noexcept {
    return m_vmid;
}
