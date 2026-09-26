// @file guestMmu.cpp
// @brief Stage-2 MMU bring-up and mapping.
// @ingroup mmu

#include "core/mm/pageTable/pageTable.h"
#include "core/mm/pmm/pmm.h"
#include "lib/log/log.h"
#include "lib/strings/strings.h"
#include "guestMmu.h"

namespace {
// 40-bit IPA with 4 KiB granules starts at concatenated L1 root tables:
// two 4 KiB tables, indexed by IPA[39:30]. This preserves the 40-bit IPA
// space without using an L0 root.
constexpr uint32_t kStage2StartLevel { 1 };
constexpr uint64_t kStage2T0sz { 24 };
constexpr uint64_t kStage2RootSize { PAGE_SIZE * 2ULL };

constexpr uint64_t kStage2RootIndexMask { 0x3FFULL }; // 10-bit index across both L1 tables

uint64_t buildStage2BlockDescriptor(uint64_t pa, bool isDevice) {
    // Check if this is device memory or normal memory
    uint64_t memAttr { isDevice ? S2PTE_MEMATTR_DEVICE_nGnRnE : S2PTE_MEMATTR_NORMAL_WB };
    uint64_t xn { isDevice ? S2PTE_XN_ALL : S2PTE_XN_NONE };

    return (pa & PTE_ADDR_MASK) | PTE_VALID | PTE_BLOCK | PTE_AF | S2PTE_SH_INNER | S2PTE_S2AP_RW |
            memAttr | xn;
}

uint64_t buildStage2PageDescriptor(uint64_t pa, bool isDevice) {
    return buildStage2BlockDescriptor(pa, isDevice) | PTE_TABLE;
}

uint64_t* allocStage2RootTable() {
    uint64_t pa { Pmm::GetInstance().AllocPages(1) };
    if (pa == 0) {
        Log::Println("[GuestMmu][ERROR] failed to allocate stage-2 root");
        for (;;)
            asm volatile("wfe");
    }

    uint64_t* table { reinterpret_cast<uint64_t*>(pa) };
    memset(table, 0, kStage2RootSize);
    PageTable::CleanDataCacheRange(table, kStage2RootSize);
    return table;
}

uint64_t* walkL3(const PageTable& table, uint64_t ipa) {
    uint64_t* l2 { table.Walk(ipa, true) };
    if (l2 == nullptr) return nullptr;

    if (!pte_is_table(*l2)) {
        uint64_t block { *l2 };
        uint64_t* l3 { PageTable::AllocTable() };
        if (pte_is_block(block)) {
            uint64_t base { block & PTE_ADDR_MASK };
            uint64_t attributes { (block & ~PTE_ADDR_MASK) | PTE_TABLE };
            for (uint64_t i {}; i < SIZE_2MB / SIZE_4KB; ++i)
                l3[i] = (base + i * SIZE_4KB) | attributes;
            PageTable::CleanDataCacheRange(l3, SIZE_4KB);
            *l2 = 0;
            PageTable::CleanDataCacheRange(l2, sizeof(*l2));
            asm volatile("dsb ish" ::: "memory");
            uint64_t hcr {};
            asm volatile("mrs %0, hcr_el2" : "=r"(hcr));
            if (hcr & 1) {
                asm volatile("tlbi vmalls12e1is\ndsb ish\nisb" ::: "memory");
            }
        }
        *l2 = reinterpret_cast<uint64_t>(l3) | PTE_VALID | PTE_TABLE;
        PageTable::CleanDataCacheRange(l2, sizeof(*l2));
    }

    if (!pte_is_table(*l2)) return nullptr;

    uint64_t* l3 { pte_next_table(*l2) };
    return &l3[L3_INDEX(ipa)];
}
} // namespace

GuestMmu::GuestMmu(
        uint64_t ipaBase, uint64_t hostPaBase, uint64_t sizeBytes, const MmioMap& devices) :
            m_rootTableOwner { allocStage2RootTable() },
            m_table { m_rootTableOwner.get(), kStage2StartLevel, kStage2RootIndexMask } {
    m_rootTable = reinterpret_cast<uint64_t>(m_rootTableOwner.get());
    Log::Println("[GuestMmu] root table={}", m_rootTableOwner.get());

    uint64_t vtcr { VTCR_T0SZ(kStage2T0sz) | VTCR_SL0_L1 | VTCR_TG0_4K | VTCR_SH0_IS |
        VTCR_ORGN0_WB | VTCR_IRGN0_WB | VTCR_PS_40BIT | VTCR_RES1 };

    Log::Println("[GuestMmu] Programming VTCR_EL2");
    asm volatile("msr vtcr_el2, %0" ::"r"(vtcr) : "memory");
    asm volatile("isb");

    Log::Println("[GuestMmu] Mapping guest IPA range");
    for (uint64_t off {}; off < sizeBytes; off += SIZE_2MB) {
        MapBlock(ipaBase + off, hostPaBase + off, false);
    }

    Log::Println("[GuestMmu] Mapping {} guest MMIO window(s)", devices.GetCount());
    for (const MmioWindow& window : devices) {
        Log::Println("[GuestMmu]   ipa {:x}..{:x} -> pa {:x}",
                window.base,
                window.base + window.size,
                window.pa);

        uint64_t granule { window.byPage ? SIZE_4KB : SIZE_2MB };
        for (uint64_t off {}; off < window.size; off += granule) {
            if (window.byPage) {
                MapPage(window.base + off, window.pa + off, true);
            } else {
                MapBlock(window.base + off, window.pa + off, true);
            }
        }
    }

    asm volatile("dsb ishst" ::: "memory");
    Log::Println("[GuestMmu] stage-2 tables built");
}

void GuestMmu::MapBlock(uint64_t ipa, uint64_t pa, bool isDevice) {
    uint64_t* pte { m_table.Walk(ipa, true) };
    if (!pte) {
        Log::Println("[ERROR] GuestMmu::mapBlock walk failed");
        return;
    }
    *pte = buildStage2BlockDescriptor(pa, isDevice);
    PageTable::CleanDataCacheRange(pte, sizeof(*pte));
}

void GuestMmu::MapPage(uint64_t ipa, uint64_t pa, bool isDevice) {
    uint64_t* pte { walkL3(m_table, ipa) };
    if (!pte) {
        Log::Println("[ERROR] GuestMmu::mapPage walk failed");
        return;
    }
    *pte = buildStage2PageDescriptor(pa, isDevice);
    PageTable::CleanDataCacheRange(pte, sizeof(*pte));
}

void GuestMmu::Enable(uint8_t vmid) {
    m_vmid = vmid;

    uint64_t rootPa { (uint64_t)(uintptr_t)m_rootTableOwner.get() };
    uint64_t vttbr { ((uint64_t)vmid << 48) | (rootPa & PTE_ADDR_MASK) };

    // Drain page-table stores to PoC before the PTW can ever read VTTBR.
    asm volatile("dsb ish" ::: "memory");

    Log::Println("[GuestMmu] Programming VTTBR_EL2");
    asm volatile("msr vttbr_el2, %0" ::"r"(vttbr) : "memory");
    asm volatile("isb");

    // Stage-2 must be enabled before tlbi vmalls12e1is can flush stage-2
    // walk-cache entries. Issuing the TLBI with HCR.VM=0 leaves any
    // speculative walks of pre-VTTBR memory cached in the walker.
    Log::Println("[GuestMmu] Setting HCR_EL2.VM");
    uint64_t hcr;
    asm volatile("mrs %0, hcr_el2" : "=r"(hcr));
    hcr |= (1ULL << 0);
    asm volatile("msr hcr_el2, %0" ::"r"(hcr) : "memory");
    asm volatile("isb");

    TlbFlushAllGuest();

    Log::Println("[GuestMmu] stage-2 enabled");
}

void GuestMmu::TlbFlushAllGuest() {
    asm volatile("tlbi vmalls12e1is" ::: "memory");
    asm volatile("dsb ish" ::: "memory");
    asm volatile("isb");
}
