/**
 * @file vm.h
 * @brief Per-guest VM container: owns a stage-2 MMU and its vCPU(s).
 * @ingroup vm
 *
 * A VM is the unit that the hypervisor schedules: one stage-2 translation
 * regime, one VMID, and the vCPUs that share both. Currently one vCPU
 * per VM; the member is named with growth in mind.
 */
#ifndef __VM_H__
#define __VM_H__

#include <stdint.h>

#include "core/mm/mmu/guestMmu/guestMmu.h"
#include "core/vcpu/vcpu.h"

class Vm {
private:
  const char* m_name = "";
  GuestMmu    m_guestMmu;
  Vcpu        m_vcpu;
  uint8_t     m_vmid;

public:
  /**
   * @brief Build this VM's stage-2 mappings and seed its vCPU.
   *
   * Order is significant: stage-2 tables are constructed first so they
   * are in place before the vCPU's first entry. VTTBR_EL2 /
   * HCR_EL2.VM are committed in @ref run(), after vCPU state is seeded.
   *
   * @param ipaBase     Guest IPA base identity-mapped to matching PA.
   * @param sizeBytes   Size of the identity-mapped region.
   * @param vmid        Non-zero VMID (unique across live VMs).
   * @param guestEntry  Guest IPA at which to resume on first @c eret.
   */
  void init(const char* name, uint64_t ipaBase, uint64_t sizeBytes, uint8_t vmid,
            uint64_t guestEntry);

  /**
   * @brief Enable stage-2 and enter the guest.
   * @note Does not return; the guest runs forever or traps back via
   *       the exception path, which is owned by vcpu.S / exceptions.S.
   */
  void run();
  
  /*
   * @brief Return Guest Kernel name
   * */
  const char* getName() const;
};

#endif  // !__VM_H__
