// @file hostMmu.h
// @brief EL2 stage-1 MMU (host self-map) interface and attribute helpers.
// @ingroup mmu
//
// Programs the EL2 translation tables that make the hypervisor itself
// executable with caches, shareability, and device-memory attributes.
// Stage-2 (guest) translation lives in @ref guestMmu.h.
#ifndef __HOST_MMU_H__
#define __HOST_MMU_H__

#include "core/mm/pageTable/pageTable.h"
#include "core/mm/mmu/mmioMap.h"

// Shareability [9:8].
#define PTE_SH_INNER (3ULL << 8)
#define PTE_SH_OUTER (2ULL << 8)
#define PTE_SH_NONE (0ULL << 8)

// Access permissions [7:6] (stage-1 EL2 encoding).
#define PTE_AP_RW (0ULL << 6)
#define PTE_AP_RO (2ULL << 6)

// Execute never [54].
#define PTE_XN (1ULL << 54)

// MAIR indices used by the EL2 self-map.
#define MAIR_IDX_NORMAL 0
#define MAIR_IDX_DEVICE 1
#define MAIR_IDX_NORMAL_NC 2

#define PTE_ATTRIDX(idx) ((uint64_t)(idx) << 2)

#define PTE_NORMAL (PTE_VALID | PTE_AF | PTE_SH_INNER | PTE_ATTRIDX(MAIR_IDX_NORMAL))
#define PTE_DEVICE (PTE_VALID | PTE_AF | PTE_SH_NONE | PTE_ATTRIDX(MAIR_IDX_DEVICE) | PTE_XN)

// Hypervisor VA layout.
#define HV_VA_BASE 0x0ULL
// Raspberry Pi firmware can place the guest package above the 8 GiB RAM
// boundary on 8 GiB boards. Keep the early EL2 identity map wide enough for
// that firmware-loaded package while the boot loader still assumes PA == VA.
#define HV_VA_SIZE SIZE_16GB

namespace HostMmu {
// @brief Build EL2 translation tables and enable stage-1 translation.
// @ingroup mmu
//
// @param devices Device windows to map as Device-nGnRnE, read out of the host
//                device tree by the caller. These overwrite the normal memory
//                blocks the hypervisor self-map lays down first, so a
//                peripheral missing from @p devices stays mapped cacheable
//                rather than faulting.
void Init(const MmioMap& devices);

// @brief Convert a host physical address to an EL2-accessible pointer.
// @ingroup mmu
//
// HyperBerry currently uses an identity direct map for RAM. Keeping this
// helper at the boundary avoids spreading that assumption through loaders.
//
// @param pa Host physical address.
// @return EL2 virtual address for the same byte.
inline void* PaToVa(uint64_t pa) {
    // TODO: Replace this identity direct-map assumption if HyperBerry moves
    // to a higher-half or otherwise non-identity host VA layout.
    return reinterpret_cast<void*>(pa);
}

// @brief Map a physical range into EL2 VA space using 2 MiB blocks.
// @ingroup mmu
// @param va    Virtual base address.
// @param pa    Physical base address.
// @param size  Mapping size in bytes (multiple of 2 MiB).
// @param flags Descriptor flags excluding the output-address bits.
void MapRange(uint64_t va, uint64_t pa, uint64_t size, uint64_t flags);

// @brief Remove mappings for a virtual range and flush affected TLBs.
// @ingroup mmu
// @param va   Virtual base address.
// @param size Range size in bytes (multiple of 2 MiB).
void UnmapRange(uint64_t va, uint64_t size);

// @brief Invalidate all EL2 stage-1 TLB entries.
// @ingroup mmu
void TlbFlushAll();

// @brief Invalidate a single EL2 VA translation from the TLB.
// @ingroup mmu
// @param va Virtual address within the page to flush.
void TlbFlushVa(uint64_t va);
} // namespace HostMmu

#endif // !__HOST_MMU_H__
