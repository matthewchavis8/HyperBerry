/**
 * @file pageTable.h
 * @brief Shared page-table primitives for stage-1 and stage-2 MMUs.
 * @ingroup mm
 *
 * Descriptor bits, index macros, and the generic walk / allocTable used
 * by both HostMmu (EL2 stage-1) and GuestMmu (stage-2). Stage-specific
 * attribute bits (MAIR index, S2AP, MemAttr) live with their owning MMU.
 */
#ifndef __PAGE_TABLE_H__
#define __PAGE_TABLE_H__

#include <stddef.h>
#include <stdint.h>

// Descriptor bit positions identical in stage-1 and stage-2 VMSAv8-64.
#define PTE_VALID (1ULL << 0)
#define PTE_TABLE (1ULL << 1)
#define PTE_BLOCK (0ULL << 1)
#define PTE_AF    (1ULL << 10)

// Output-address mask (bits [47:12]).
#define PTE_ADDR_MASK 0x0000FFFFFFFFF000ULL

// Granule and block sizes.
#define SIZE_4KB 0x1000ULL
#define SIZE_2MB 0x200000ULL
#define SIZE_1GB 0x40000000ULL
#define SIZE_8GB 0x200000000ULL

// Table-index extraction from an address.
#define L0_INDEX(addr) (((addr) >> 39) & 0x1FFULL)
#define L1_INDEX(addr) (((addr) >> 30) & 0x1FFULL)
#define L2_INDEX(addr) (((addr) >> 21) & 0x1FFULL)
#define L3_INDEX(addr) (((addr) >> 12) & 0x1FFULL)

static inline uint64_t* pte_next_table(uint64_t entry) {
  return (uint64_t*)(uintptr_t)(entry & PTE_ADDR_MASK);
}

static inline int pte_is_valid(uint64_t entry) {
  return (int)(entry & PTE_VALID);
}

static inline int pte_is_table(uint64_t entry) {
  return (entry & (PTE_VALID | PTE_TABLE)) == (PTE_VALID | PTE_TABLE);
}

static inline int pte_is_block(uint64_t entry) {
  return (entry & (PTE_VALID | PTE_TABLE)) == PTE_VALID;
}

namespace PageTable {
  /**
   * @brief Walker configuration: where to start and whether to grow the
   *        table on a missing intermediate entry.
   * @ingroup mm
   *
   * `startLevel == 0` corresponds to stage-1 with T0SZ=16 (48-bit VA).
   * `startLevel == 1` corresponds to stage-2 with T0SZ=32 (32-bit IPA).
   * Both walks terminate at the L2 entry; callers map 2 MiB blocks.
   */
  struct WalkConfig {
    uint32_t startLevel;
    bool     allocOnMiss;
  };

  /**
   * @brief Allocate a zeroed 4 KiB table from the PMM.
   * @ingroup mm
   * @return Pointer to the fresh table. Never returns null — halts on
   *         allocation failure so a missing page does not silently
   *         produce bogus translations.
   */
  uint64_t* allocTable();

  /**
   * @brief Walk a multi-level translation table and return the L2 entry.
   * @ingroup mm
   * @param root Root table pointer (stage-1 L0 or stage-2 L1).
   * @param addr Address to resolve (VA for stage-1, IPA for stage-2).
   * @param cfg  Starting level and allocation behaviour.
   * @return Pointer to the L2 entry covering @p addr, or nullptr when
   *         allocOnMiss is false and an intermediate table is absent.
   */
  uint64_t* walk(uint64_t* root, uint64_t addr, const WalkConfig& cfg);
}

#endif  // !__PAGE_TABLE_H__
