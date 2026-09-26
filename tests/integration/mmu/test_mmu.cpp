// @file test_mmu.cpp
// @brief Integration tests for EL2 MMU mappings and runtime APIs.

#include "regs.inc"
#include "core/mm/mmu/hostMmu/hostMmu.h"
#include "tests/integration/suite.h"

namespace {
constexpr uint64_t kTestVa { 0x2000000000ULL };
constexpr uint64_t kTestPa { 0x08000000ULL };

uint64_t* rootTable() {
    uint64_t ttbr0;
    asm volatile("mrs %0, ttbr0_el2" : "=r"(ttbr0));
    return reinterpret_cast<uint64_t*>(ttbr0 & PTE_ADDR_MASK);
}

uint64_t* walkToL2Entry(uint64_t va) {
    uint64_t* l0 { rootTable() };
    if (!l0) return nullptr;

    uint64_t l0_entry { l0[L0_INDEX(va)] };
    if (!pte_is_table(l0_entry)) return nullptr;

    uint64_t* l1 { pte_next_table(l0_entry) };
    uint64_t l1_entry { l1[L1_INDEX(va)] };
    if (!pte_is_table(l1_entry)) return nullptr;

    uint64_t* l2 { pte_next_table(l1_entry) };
    return &l2[L2_INDEX(va)];
}

bool entryMatches(uint64_t* entry, uint64_t pa, uint64_t flags) {
    if (!entry) return false;

    uint64_t value { *entry };
    if (!pte_is_block(value)) return false;

    return (value & PTE_ADDR_MASK) == (pa & PTE_ADDR_MASK) &&
            (value & ~PTE_ADDR_MASK) == (flags | PTE_BLOCK);
}
} // namespace

static bool test_ttbr0_present_and_page_aligned() {
    uint64_t ttbr0;
    asm volatile("mrs %0, ttbr0_el2" : "=r"(ttbr0));
    return ttbr0 != 0 && (ttbr0 & (SIZE_4KB - 1)) == 0;
}

static bool test_hv_identity_mapping_present() {
    return entryMatches(walkToL2Entry(HV_VA_BASE), HV_VA_BASE, PTE_NORMAL | PTE_AP_RW);
}

static bool test_console_mapped_as_device() {
    // The window a fixed HV_MMIO range could silently omit, which left EL2
    // driving its own console through the cacheable self-map.
    uint64_t block { BSP_UART_BASE & ~(SIZE_2MB - 1) };
    return entryMatches(walkToL2Entry(block), block, PTE_DEVICE);
}

static bool test_gic_distributor_mapped_as_device() {
    uint64_t block { BSP_GIC_DISTRIBUTOR_BASE & ~(SIZE_2MB - 1) };
    return entryMatches(walkToL2Entry(block), block, PTE_DEVICE);
}

static bool test_map_range_installs_block_entry() {
    HostMmu::GetInstance().MapRange(kTestVa, kTestPa, SIZE_2MB, PTE_NORMAL | PTE_AP_RW);
    return entryMatches(walkToL2Entry(kTestVa), kTestPa, PTE_NORMAL | PTE_AP_RW);
}

static bool test_unmap_range_clears_block_entry() {
    HostMmu::GetInstance().MapRange(kTestVa, kTestPa, SIZE_2MB, PTE_NORMAL | PTE_AP_RW);
    HostMmu::GetInstance().UnmapRange(kTestVa, SIZE_2MB);

    uint64_t* entry { walkToL2Entry(kTestVa) };
    return entry && *entry == 0;
}

static bool test_tlb_flush_apis_do_not_hang() {
    HostMmu::TlbFlushVa(HV_VA_BASE);
    HostMmu::TlbFlushAll();
    return true;
}

static const TestCase kMmuCases[] {
    { "ttbr0_present_and_page_aligned", test_ttbr0_present_and_page_aligned },
    { "hv_identity_mapping_present", test_hv_identity_mapping_present },
    { "console_mapped_as_device", test_console_mapped_as_device },
    { "gic_distributor_mapped_as_device", test_gic_distributor_mapped_as_device },
    { "map_range_installs_block_entry", test_map_range_installs_block_entry },
    { "unmap_range_clears_block_entry", test_unmap_range_clears_block_entry },
    { "tlb_flush_apis_do_not_hang", test_tlb_flush_apis_do_not_hang },
};

static const TestSuite kMmuSuite {
    "MmuHarness",
    kMmuCases,
    7,
};

REGISTER_SUITE(kMmuSuite);
