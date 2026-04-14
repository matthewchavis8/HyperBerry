/**
 * @file test_mmu.cpp
 * @brief Integration tests for EL2 MMU mappings and runtime APIs.
 */

#include "core/mm/mmu/mmu.h"
#include "tests/integration/suite.h"

namespace {
  constexpr uint64_t kTestVa = 0x2000000000ULL;
  constexpr uint64_t kTestPa = 0x08000000ULL;

  uint64_t* rootTable() {
    uint64_t ttbr0;
    asm volatile("mrs %0, ttbr0_el2" : "=r"(ttbr0));
    return reinterpret_cast<uint64_t*>(ttbr0 & PTE_ADDR_MASK);
  }

  uint64_t* walkToL2Entry(uint64_t va) {
    uint64_t* l0 = rootTable();
    if (!l0)
      return nullptr;

    uint64_t l0_entry = l0[L0_INDEX(va)];
    if (!pte_is_table(l0_entry))
      return nullptr;

    uint64_t* l1 = pte_next_table(l0_entry);
    uint64_t l1_entry = l1[L1_INDEX(va)];
    if (!pte_is_table(l1_entry))
      return nullptr;

    uint64_t* l2 = pte_next_table(l1_entry);
    return &l2[L2_INDEX(va)];
  }

  bool entryMatches(uint64_t* entry, uint64_t pa, uint64_t flags) {
    if (!entry)
      return false;

    uint64_t value = *entry;
    if (!pte_is_block(value))
      return false;

    return (value & PTE_ADDR_MASK) == (pa & PTE_ADDR_MASK)
        && (value & ~PTE_ADDR_MASK) == (flags | PTE_BLOCK);
  }
}

static bool test_ttbr0_present_and_page_aligned() {
  uint64_t ttbr0;
  asm volatile("mrs %0, ttbr0_el2" : "=r"(ttbr0));
  return ttbr0 != 0 && (ttbr0 & (SIZE_4KB - 1)) == 0;
}

static bool test_hv_identity_mapping_present() {
  return entryMatches(walkToL2Entry(HV_VA_BASE), HV_VA_BASE, PTE_NORMAL | PTE_AP_RW);
}

static bool test_peripheral_mapping_present() {
  return entryMatches(walkToL2Entry(BCM2712_PERIPH_BASE),
                      BCM2712_PERIPH_BASE,
                      PTE_DEVICE);
}

static bool test_map_range_installs_block_entry() {
  mmu::mapRange(kTestVa, kTestPa, SIZE_2MB, PTE_NORMAL | PTE_AP_RW);
  return entryMatches(walkToL2Entry(kTestVa), kTestPa, PTE_NORMAL | PTE_AP_RW);
}

static bool test_unmap_range_clears_block_entry() {
  mmu::mapRange(kTestVa, kTestPa, SIZE_2MB, PTE_NORMAL | PTE_AP_RW);
  mmu::unmapRange(kTestVa, SIZE_2MB);

  uint64_t* entry = walkToL2Entry(kTestVa);
  return entry && *entry == 0;
}

static bool test_tlb_flush_apis_do_not_hang() {
  mmu::tlbFlushVa(HV_VA_BASE);
  mmu::tlbFlushAll();
  return true;
}

static const TestCase kMmuCases[] = {
    {"ttbr0_present_and_page_aligned", test_ttbr0_present_and_page_aligned},
    {"hv_identity_mapping_present",    test_hv_identity_mapping_present},
    {"peripheral_mapping_present",     test_peripheral_mapping_present},
    {"map_range_installs_block_entry", test_map_range_installs_block_entry},
    {"unmap_range_clears_block_entry", test_unmap_range_clears_block_entry},
    {"tlb_flush_apis_do_not_hang",     test_tlb_flush_apis_do_not_hang},
};

static const TestSuite kMmuSuite = {
    "MmuHarness",
    kMmuCases,
    6,
};

REGISTER_SUITE(kMmuSuite);
