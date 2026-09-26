// @file vm.h
// @brief Per-guest VM container: owns a stage-2 MMU and its vCPU(s).
// @ingroup vm
//
// A VM is the unit that the hypervisor schedules: one stage-2 translation
// regime, one VMID, and the vCPUs that share both. Currently one vCPU
// per VM; the member is named with growth in mind.
#ifndef __VM_H__
#define __VM_H__

#include <cstdint>
#include <string_view>

#include "core/mm/mmu/guestMmu/guestMmu.h"
#include "core/vcpu/vcpu.h"

// @brief Where a guest lives and how it starts.
// @ingroup vm
struct VmConfig {
    std::string_view name; // guest kernel or VM name, for diagnostics
    uint64_t ipaBase;      // guest IPA base
    uint64_t ramHostPa;    // host physical base backing guest RAM
    uint64_t ramSize;      // size of the guest RAM region
    uint8_t vmid;          // non-zero, unique across live VMs
    uint64_t entry;        // guest IPA to resume at on first eret
    uint64_t dtb;          // guest IPA of the Linux device tree blob
};

class Vm {
private:
    std::string_view m_name;
    GuestMmu m_guestMmu;
    Vcpu m_vcpu;
    uint8_t m_vmid;

public:
    // @brief Build this VM's stage-2 mappings and seed its vCPU.
    //
    // Member order is significant: the stage-2 tables are constructed first
    // so they are in place before the vCPU's first entry. VTTBR_EL2 /
    // HCR_EL2.VM are committed in @ref Run(), after vCPU state is seeded.
    //
    // @param config  Guest placement and entry state.
    // @param devices Device windows this guest may reach.
    Vm(const VmConfig& config, const MmioMap& devices);

    // @brief Enable stage-2 and enter the guest.
    // @note Does not return; the guest runs forever or traps back via
    //       the exception path, which is owned by vcpu.S / vmm.S.
    // @return Nothing.
    void Run();

    // @return The guest name passed in @ref VmConfig.
    [[nodiscard]] std::string_view GetName() const noexcept;

    // @return The VMID passed in @ref VmConfig.
    [[nodiscard]] uint8_t GetVmId() const noexcept;
};

#endif // !__VM_H__
