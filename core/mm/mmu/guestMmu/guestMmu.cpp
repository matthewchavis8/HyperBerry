/**
 * @file guestMmu.cpp
 * @brief Stage-2 MMU bring-up and mapping.
 * @ingroup mmu
 */

#include "core/mm/pageTable/pageTable.h"
#include "core/mm/pmm/pmm.h"
#include "lib/log/log.h"
#include "lib/strings/strings.h"
#include "guestMmu.h"

namespace {
// 40-bit IPA with 4 KiB granules starts at concatenated L1 root tables:
// two 4 KiB tables, indexed by IPA[39:30]. This preserves the 40-bit IPA
// space without using an L0 root.
constexpr uint32_t kStage2StartLevel = 1;
constexpr uint64_t kStage2T0sz = 24;
constexpr uint64_t kStage2RootSize = PAGE_SIZE * 2ULL;

constexpr PageTable::WalkConfig kStage2Walk {
    kStage2StartLevel,
    0x3FFULL, // 10-bit root index across two concatenated L1 tables.
    true,
};

uint64_t buildStage2BlockDescriptor(uint64_t pa, bool isDevice) {
    // Check if this is device memory or normal memory
    uint64_t memAttr = isDevice ? S2PTE_MEMATTR_DEVICE_nGnRnE : S2PTE_MEMATTR_NORMAL_WB;
    uint64_t xn = isDevice ? S2PTE_XN_ALL : S2PTE_XN_NONE;

    return (pa & PTE_ADDR_MASK) | PTE_VALID | PTE_BLOCK | PTE_AF | S2PTE_SH_INNER | S2PTE_S2AP_RW |
            memAttr | xn;
}

uint64_t buildStage2PageDescriptor(uint64_t pa, bool isDevice) {
    return buildStage2BlockDescriptor(pa, isDevice) | PTE_TABLE;
}

uint64_t* allocStage2RootTable() {
    uint64_t pa = pmm::allocPages(1);
    if (pa == 0) {
        Log::println("[GuestMmu][ERROR] failed to allocate stage-2 root");
        for (;;)
            asm volatile("wfe");
    }

    uint64_t* table = reinterpret_cast<uint64_t*>(pa);
    memset(table, 0, kStage2RootSize);
    PageTable::cleanDataCacheRange(table, kStage2RootSize);
    return table;
}

uint64_t* walkL3(uint64_t* root, uint64_t ipa) {
    uint64_t* l2 = PageTable::walk(root, ipa, kStage2Walk);
    if (l2 == nullptr) return nullptr;

    if (!pte_is_valid(*l2)) {
        uint64_t* l3 = PageTable::allocTable();
        *l2 = reinterpret_cast<uint64_t>(l3) | PTE_VALID | PTE_TABLE;
        PageTable::cleanDataCacheRange(l2, sizeof(*l2));
    }

    if (!pte_is_table(*l2)) return nullptr;

    uint64_t* l3 = pte_next_table(*l2);
    return &l3[L3_INDEX(ipa)];
}
} // namespace

void GuestMmu::init(
        uint64_t ipaBase, uint64_t hostPaBase, uint64_t sizeBytes, const MmioMap& devices) {
    Log::println("[GuestMmu] init called");
    m_rootTableOwner.reset(allocStage2RootTable());
    m_rootTable = reinterpret_cast<uint64_t>(m_rootTableOwner.get());
    Log::println("[GuestMmu] root table={}", m_rootTableOwner.get());

    uint64_t vtcr = VTCR_T0SZ(kStage2T0sz) | VTCR_SL0_L1 | VTCR_TG0_4K | VTCR_SH0_IS |
            VTCR_ORGN0_WB | VTCR_IRGN0_WB | VTCR_PS_40BIT | VTCR_RES1;

    Log::println("[GuestMmu] Programming VTCR_EL2");
    asm volatile("msr vtcr_el2, %0" ::"r"(vtcr) : "memory");
    asm volatile("isb");

    Log::println("[GuestMmu] Mapping guest IPA range");
    for (uint64_t off {}; off < sizeBytes; off += SIZE_2MB) {
        mapBlock(ipaBase + off, hostPaBase + off, false);
    }

    Log::println("[GuestMmu] Mapping {} guest MMIO window(s)", devices.count);
    for (uint32_t i {}; i < devices.count; ++i) {
        const MmioWindow& window = devices.windows[i];
        Log::println("[GuestMmu]   ipa {:x}..{:x} -> pa {:x}",
                window.base,
                window.base + window.size,
                window.pa);

        uint64_t granule = window.byPage ? SIZE_4KB : SIZE_2MB;
        for (uint64_t off {}; off < window.size; off += granule) {
            if (window.byPage) {
                mapPage(window.base + off, window.pa + off, true);
            } else {
                mapBlock(window.base + off, window.pa + off, true);
            }
        }
    }

    asm volatile("dsb ishst" ::: "memory");
    Log::println("[GuestMmu] init finished");
}

void GuestMmu::mapBlock(uint64_t ipa, uint64_t pa, bool isDevice) {
    uint64_t* pte = PageTable::walk(m_rootTableOwner.get(), ipa, kStage2Walk);
    if (!pte) {
        Log::println("[ERROR] GuestMmu::mapBlock walk failed");
        return;
    }
    *pte = buildStage2BlockDescriptor(pa, isDevice);
    PageTable::cleanDataCacheRange(pte, sizeof(*pte));
}

void GuestMmu::mapPage(uint64_t ipa, uint64_t pa, bool isDevice) {
    uint64_t* pte = walkL3(m_rootTableOwner.get(), ipa);
    if (!pte) {
        Log::println("[ERROR] GuestMmu::mapPage walk failed");
        return;
    }
    *pte = buildStage2PageDescriptor(pa, isDevice);
    PageTable::cleanDataCacheRange(pte, sizeof(*pte));
}

void GuestMmu::enable(uint8_t vmid) {
    m_vmid = vmid;

    uint64_t rootPa = (uint64_t)(uintptr_t)m_rootTableOwner.get();
    uint64_t vttbr = ((uint64_t)vmid << 48) | (rootPa & PTE_ADDR_MASK);

    // Drain page-table stores to PoC before the PTW can ever read VTTBR.
    asm volatile("dsb ish" ::: "memory");

    Log::println("[GuestMmu] Programming VTTBR_EL2");
    asm volatile("msr vttbr_el2, %0" ::"r"(vttbr) : "memory");
    asm volatile("isb");

    // Stage-2 must be enabled before tlbi vmalls12e1is can flush stage-2
    // walk-cache entries. Issuing the TLBI with HCR.VM=0 leaves any
    // speculative walks of pre-VTTBR memory cached in the walker.
    Log::println("[GuestMmu] Setting HCR_EL2.VM");
    uint64_t hcr;
    asm volatile("mrs %0, hcr_el2" : "=r"(hcr));
    hcr |= (1ULL << 0);
    asm volatile("msr hcr_el2, %0" ::"r"(hcr) : "memory");
    asm volatile("isb");

    tlbFlushAllGuest();

    Log::println("[GuestMmu] stage-2 enabled");
}

void GuestMmu::tlbFlushAllGuest() {
    asm volatile("tlbi vmalls12e1is" ::: "memory");
    asm volatile("dsb ish" ::: "memory");
    asm volatile("isb");
}
