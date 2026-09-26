// @file vm.cpp
// @brief Per-guest VM container implementation.
// @ingroup vm

#include "core/mm/pmm/pmm.h"
#include "lib/log/log.h"
#include "vm.h"

Vm::Vm(const char* name,
        uint64_t ipaBase,
        uint64_t guestRamHostPa,
        uint64_t sizeBytes,
        uint8_t vmid,
        uint64_t guestEntry,
        uint64_t guestDtb,
        const MmioMap& devices) :
            m_name { name },
            m_guestMmu { ipaBase, guestRamHostPa, sizeBytes, devices },
            m_vcpu { guestEntry },
            m_vmid { vmid } {
    m_vcpu.SetGpReg(VCPU_GPREG_X0, guestDtb);
    Log::Println("[VM] {} built", m_name);
}

void Vm::Run() {
    Log::Println("[VM] Enabling Guest MMU");
    m_guestMmu.Enable(m_vmid);
    Log::Println("[VM] Successfully enabled Guest MMU");

    Log::Println("[VM] Guest Kernel Running");
    vcpu_enter(&m_vcpu);
}

[[nodiscard]] const char* Vm::GetName() const noexcept {
    return m_name;
}

[[nodiscard]] uint8_t Vm::GetVmId() const noexcept {
    return m_vmid;
}
