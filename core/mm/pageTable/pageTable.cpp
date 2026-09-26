// @file pageTable.cpp
// @brief Shared page-table walk and allocation.
// @ingroup mm

#include "core/mm/pmm/pmm.h"
#include "lib/log/log.h"
#include "lib/strings/strings.h"
#include "pageTable.h"

void PageTable::CleanDataCacheRange(const void* addr, size_t size) {
#if defined(__aarch64__)
    if (size == 0) return;

    uint64_t ctr {};
    asm volatile("mrs %0, ctr_el0" : "=r"(ctr));

    uint64_t lineSize { 4ULL << ((ctr >> 16) & 0xFULL) };
    uint64_t start { reinterpret_cast<uint64_t>(addr) & ~(lineSize - 1ULL) };
    uint64_t end { reinterpret_cast<uint64_t>(addr) + size };

    for (uint64_t line { start }; line < end; line += lineSize) {
        asm volatile("dc cvac, %0" ::"r"(line) : "memory");
    }
    asm volatile("dsb ishst" ::: "memory");
#else
    (void)addr;
    (void)size;
#endif
}

uint64_t* PageTable::AllocTable() {
    uint64_t pa { pmm::AllocPages(0) };
    if (pa == 0) {
        Log::Println("[PageTable] Failed to allocate page table");
        for (;;)
            asm volatile("wfe");
    }

    uint64_t* table { reinterpret_cast<uint64_t*>(pa) };
    memset(table, 0, PAGE_SIZE);
    CleanDataCacheRange(table, PAGE_SIZE);
    return table;
}

uint64_t* PageTable::Walk(uint64_t addr, bool allocOnMiss) const {
    static constexpr uint32_t levelShift[] { 39, 30, 21, 12 };

    uint64_t* table { m_root };

    for (uint32_t level { m_startLevel }; level < 2; ++level) {
        uint64_t mask { (level == m_startLevel) ? m_rootIndexMask : 0x1FFULL };
        uint64_t idx { (addr >> levelShift[level]) & mask };

        if (!pte_is_valid(table[idx])) {
            if (!allocOnMiss) return nullptr;

            uint64_t* next { AllocTable() };
            table[idx] = (uint64_t)(uintptr_t)next | PTE_VALID | PTE_TABLE;
            CleanDataCacheRange(&table[idx], sizeof(table[idx]));
        }

        table = pte_next_table(table[idx]);
    }

    return &table[L2_INDEX(addr)];
}
