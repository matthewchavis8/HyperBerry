// @file hostMmu.cpp
// @brief EL2 stage-1 MMU bring-up and mapping operations.
// @ingroup mmu

#include "core/mm/pageTable/pageTable.h"
#include "lib/log/log.h"
#include "hostMmu.h"

namespace {
constexpr uint32_t STAGE1_START_LEVEL { 0 };
constexpr uint64_t STAGE1_ROOT_INDEX_MASK { 0x1FFULL };
} // namespace

HostMmu::HostMmu() :
            m_table { PageTable::AllocTable(), STAGE1_START_LEVEL, STAGE1_ROOT_INDEX_MASK } {}

HostMmu& HostMmu::GetInstance() {
    static HostMmu hostMmu;
    return hostMmu;
}

void HostMmu::Enable(const MmioMap& devices) {
    Log::Println("[HostMmu] L0 table={}", m_table.GetRoot());

    Log::Println("[HostMmu] Programming MAIR");
    uint64_t mair { (0xFFULL << (MAIR_IDX_NORMAL * 8)) // Normal memory
        | (0x00ULL << (MAIR_IDX_DEVICE * 8))           // Device memory
        | (0x44ULL << (MAIR_IDX_NORMAL_NC * 8)) };     // Normal but non cacheable memory
    asm volatile("msr mair_el2, %0" ::"r"(mair) : "memory");

    Log::Println("[HostMmu] Programming TCR");
    uint64_t tcr { (16ULL << 0) | (0ULL << 14) | (1ULL << 8) | (1ULL << 10) | (3ULL << 12) |
        (2ULL << 16) };
    asm volatile("msr tcr_el2, %0" ::"r"(tcr) : "memory");
    asm volatile("isb");

    Log::Println("[HostMmu] Mapping HV DRAM");
    MapRange(HV_VA_BASE, HV_VA_BASE, HV_VA_SIZE, PTE_NORMAL | PTE_AP_RW);

    Log::Println("[HostMmu] Mapping {} HV MMIO window(s)", devices.GetCount());
    for (const MmioWindow& window : devices) {
        Log::Println("[HostMmu]   {:x}..{:x}", window.base, window.base + window.size);
        MapRange(window.base, window.pa, window.size, PTE_DEVICE);
    }

    Log::Println("[HostMmu] Programming TTBR0");
    asm volatile("msr ttbr0_el2, %0" ::"r"((uint64_t)(uintptr_t)m_table.GetRoot()) : "memory");
    asm volatile("dsb ishst" ::: "memory");
    asm volatile("isb");

    Log::Println("[HostMmu] Enabling SCTLR.M/C/I");
    uint64_t sctlr;
    asm volatile("mrs %0, sctlr_el2" : "=r"(sctlr));
    sctlr |= (1ULL << 0)    // enable MMU
            | (1ULL << 2)   // enable data cache
            | (1ULL << 12); // enable instruction cache
    asm volatile("msr sctlr_el2, %0" ::"r"(sctlr) : "memory");
    asm volatile("isb");

    Log::Println("[HostMmu] stage-1 enabled");
}

void HostMmu::MapRange(uint64_t va, uint64_t pa, uint64_t size, uint64_t flags) {
    for (uint64_t off {}; off < size; off += SIZE_2MB) {
        uint64_t* pte { m_table.Walk(va + off, true) };
        if (!pte) {
            Log::Println("[ERROR] HostMmu::mapRange walk failed");
            break;
        }
        *pte = (((pa + off) & PTE_ADDR_MASK) | flags | PTE_BLOCK);
    }
}

void HostMmu::UnmapRange(uint64_t va, uint64_t size) {
    for (uint64_t off {}; off < size; off += SIZE_2MB) {
        uint64_t* pte { m_table.Walk(va + off, false) };
        if (!pte) {
            Log::Println("[ERROR] HostMmu::unmapRange walk failed");
            break;
        }
        *pte = 0;
        TlbFlushVa(va + off);
    }
}

void HostMmu::TlbFlushAll() {
    asm volatile("tlbi alle2is" ::: "memory");
    asm volatile("dsb sy" ::: "memory");
    asm volatile("isb");
}

void HostMmu::TlbFlushVa(uint64_t va) {
    asm volatile("tlbi vae2is, %0" ::"r"(va >> 12) : "memory");
    asm volatile("dsb sy" ::: "memory");
    asm volatile("isb");
}
