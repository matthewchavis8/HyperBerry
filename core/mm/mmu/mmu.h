#ifndef __MMU_H__
#define __MMU_H__

#include <stddef.h>
#include <stdint.h>

// Table Descriptor [1:0]
#define PTE_VALID (1ULL << 0)
#define PTE_TABLE (1ULL << 1)
#define PTE_BLOCK (0ULL << 1)

// Access Flag [10]
#define PTE_AF (1ULL << 10)

// Shareability [9:8]
#define PTE_SH_INNER (3ULL << 8)
#define PTE_SH_OUTER (2ULL << 8)
#define PTE_SH_NONE  (0ULL << 8)

// Access Permissions [7:6]
#define PTE_AP_RW (0ULL << 6)
#define PTE_AP_RO (2ULL << 6)

// Execute never [54]
#define PTE_XN    (1ULL << 54)

// Memory Attribute Index
#define MAIR_IDX_NORMAL    0 // Normal memory
#define MAIR_IDX_DEVICE    1 // Device memory
#define MAIR_IDX_NORMAL_NC 2 // Normal memory but non-cacheable

#define PTE_ATTRIDX(idx) ((uint64_t)(idx) << 2)

#define PTE_NORMAL      (PTE_VALID | PTE_AF | PTE_SH_INNER | PTE_ATTRIDX(MAIR_IDX_NORMAL))
#define PTE_DEVICE      (PTE_VALID | PTE_AF | PTE_SH_NONE  | PTE_ATTRIDX(MAIR_IDX_DEVICE) | PTE_XN)

// Output address mask
#define PTE_ADDR_MASK 0x0000FFFFFFFFF000ULL

// SIZES
#define SIZE_4KB 0x1000ULL
#define SIZE_2MB 0x200000ULL
#define SIZE_1GB 0x40000000ULL
#define SIZE_8GB 0x200000000ULL

// Virtual Address Layout
#define HV_VA_BASE 0x0ULL
#define HV_VA_SIZE SIZE_8GB

#define BCM2712_PERIPH_BASE     0x107D000000ULL
#define BCM2712_PERIPH_SIZE     0x200000000ULL

// Table Index
#define L0_INDEX(va)    (((va) >> 39) & 0x1FFULL)
#define L1_INDEX(va)    (((va) >> 30) & 0x1FFULL)
#define L2_INDEX(va)    (((va) >> 21) & 0x1FFULL)
#define L3_INDEX(va)    (((va) >> 12) & 0x1FFULL)

static inline uint64_t *pte_next_table(uint64_t entry) {
  return (uint64_t *)(uintptr_t)(entry & PTE_ADDR_MASK);
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

namespace mmu {
  void init();
  void mapRange(uint64_t va, uint64_t pa, uint64_t size, uint64_t flags);
  void unmapRange(uint64_t va, uint64_t size);
  void tlbFlushAll();
  void tlbFlushVa(uint64_t va);
}

#endif // !__MMU_H__
