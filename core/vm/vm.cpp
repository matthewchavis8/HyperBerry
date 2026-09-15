// @file vm.cpp
// @brief Per-guest VM container implementation.
// @ingroup vm

#include "core/mm/pmm/pmm.h"
#include "lib/log/log.h"
#include "vm.h"

void Vm::Init(const char* name,
        uint64_t ipaBase,
        uint64_t guestRamHostPa,
        uint64_t sizeBytes,
        uint8_t vmid,
        uint64_t guestEntry,
        uint64_t guestDtb,
        const MmioMap& devices) {
    m_name = name;
    m_vmid = vmid;

    Log::Println("[VM] Bringing up Guest MMU");
    m_guestMmu.Init(ipaBase, guestRamHostPa, sizeBytes, devices);
    Log::Println("[VM] Bringing up Guest MMU");

    Log::Println("[VM] Bringing up Vcpu");
    m_vcpu.Init(guestEntry);
    m_vcpu.SetGpReg(VCPU_GPREG_X0, guestDtb);
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
