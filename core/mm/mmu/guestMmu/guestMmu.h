/**
 * @file guestMmu.h
 * @brief Stage-2 (IPA -> PA) guest translation for EL2 hypervisor.
 * @ingroup mmu
 *
 * Each VM owns one GuestMmu. When enabled, all guest memory accesses are
 * translated from IPA to PA by stage-2 while HCR_EL2.VM=1. The EL2
 * self-map (HostMmu) is stage-1 and is orthogonal to this path.
 */
#ifndef __GUEST_MMU_H__
#define __GUEST_MMU_H__

#include "core/mm/pageTable/pageTable.h"

// Stage-2 access permissions [7:6] (flat RWX, no EL0/EL1 split).
#define S2PTE_S2AP_NONE (0b00ULL << 6)
#define S2PTE_S2AP_RO   (0b01ULL << 6)
#define S2PTE_S2AP_WO   (0b10ULL << 6)
#define S2PTE_S2AP_RW   (0b11ULL << 6)

// Stage-2 inline memory attributes [5:2] (not a MAIR index).
#define S2PTE_MEMATTR_DEVICE_nGnRnE (0x0ULL << 2)
#define S2PTE_MEMATTR_NORMAL_WB     (0xFULL << 2)

// Shareability [9:8] and execute-never [54:53] for stage-2.
#define S2PTE_SH_INNER (3ULL << 8)
#define S2PTE_XN_NONE  (0ULL << 53)
#define S2PTE_XN_ALL   (2ULL << 53)

// VTCR_EL2 field builders.
#define VTCR_T0SZ(n)    ((uint64_t)(n) & 0x3FULL)
#define VTCR_SL0_L1     (0b01ULL << 6)
#define VTCR_TG0_4K     (0b00ULL << 14)
#define VTCR_SH0_IS     (0b11ULL << 12)
#define VTCR_ORGN0_WB   (0b01ULL << 10)
#define VTCR_IRGN0_WB   (0b01ULL << 8)
#define VTCR_PS_40BIT   (0b010ULL << 16)
#define VTCR_RES1       (1ULL << 31)

/**
 * @brief Per-guest stage-2 MMU.
 * @ingroup mmu
 *
 * Owns the stage-2 root table and VMID. @ref enable() programs VTTBR_EL2
 * and sets HCR_EL2.VM=1. Shared by all vCPUs that belong to the same
 * guest (ARM semantics: one stage-2 table per VM, distinguished at the
 * TLB by VMID).
 */
class GuestMmu {
private:
  uint64_t* m_rootTable;
  uint8_t   m_vmid;

public:
  /**
   * @brief Allocate the stage-2 root table, program VTCR_EL2, and
   *        map a contiguous guest IPA range to host physical memory.
   * @param ipaBase    Start of the guest IPA region.
   * @param hostPaBase Start of the backing host physical region.
   * @param sizeBytes  Size of the region (must be 2 MiB aligned).
   */
  void init(uint64_t ipaBase, uint64_t hostPaBase, uint64_t sizeBytes);

  /**
   * @brief Install one 2 MiB stage-2 block descriptor.
   * @param ipa      Intermediate physical address (2 MiB aligned).
   * @param pa       Target physical address.
   * @param isDevice True selects device-nGnRnE memory attributes,
   *                 false selects normal write-back cacheable memory.
   */
  void mapBlock(uint64_t ipa, uint64_t pa, bool isDevice);

  /**
   * @brief Install one 4 KiB stage-2 page descriptor.
   * @param ipa      Intermediate physical address (4 KiB aligned).
   * @param pa       Target physical address (4 KiB aligned).
   * @param isDevice True selects device-nGnRnE memory attributes,
   *                 false selects normal write-back cacheable memory.
   */
  void mapPage(uint64_t ipa, uint64_t pa, bool isDevice);

  /**
   * @brief Commit this stage-2 context: program VTTBR_EL2 with @p vmid,
   *        flush stage-2 TLB, and set HCR_EL2.VM=1.
   * @param vmid Non-zero VMID. Each live VM must have a unique VMID.
   */
  void enable(uint8_t vmid);

  /** @brief Invalidate all stage-1 & stage-2 TLB entries for this VMID. */
  void tlbFlushAllGuest();
};

#endif  // !__GUEST_MMU_H__
