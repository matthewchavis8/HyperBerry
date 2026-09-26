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

#include "core/mm/mmu/guestMmu/guestMmu.h"
#include "core/vcpu/vcpu.h"

class Vm {
private:
    const char* m_name;
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
    // @param name           Guest kernel or VM name retained for diagnostics.
    // @param ipaBase        Guest IPA base.
    // @param guestRamHostPa Host physical base backing guest RAM.
    // @param sizeBytes      Size of the guest RAM region.
    // @param vmid           Non-zero VMID (unique across live VMs).
    // @param guestEntry     Guest IPA at which to resume on first @c eret.
    // @param guestDtb       Guest IPA of the Linux device tree blob.
    // @param devices        Device windows this guest may reach.
    Vm(const char* name,
            uint64_t ipaBase,
            uint64_t guestRamHostPa,
            uint64_t sizeBytes,
            uint8_t vmid,
            uint64_t guestEntry,
            uint64_t guestDtb,
            const MmioMap& devices);

    // @brief Enable stage-2 and enter the guest.
    // @note Does not return; the guest runs forever or traps back via
    //       the exception path, which is owned by vcpu.S / vmm.S.
    void Run();

    // @brief Return the guest kernel name passed to the constructor.
    [[nodiscard]] const char* GetName() const noexcept;

    // @brief Return the guest vm ID passed to the constructor.
    [[nodiscard]] uint8_t GetVmId() const noexcept;
};

#endif // !__VM_H__
