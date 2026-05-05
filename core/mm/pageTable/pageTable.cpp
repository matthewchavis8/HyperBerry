/**
 * @file pageTable.cpp
 * @brief Shared page-table walk and allocation.
 * @ingroup mm
 */

#include "core/mm/pmm/pmm.h"
#include "drivers/uart/uart.h"
#include "lib/strings/strings.h"
#include "pageTable.h"

namespace PageTable {
  void cleanDataCacheRange(const void* addr, size_t size) {
#if defined(__aarch64__)
    if (size == 0)
      return;

    uint64_t ctr {};
    asm volatile("mrs %0, ctr_el0" : "=r"(ctr));

    uint64_t lineSize = 4ULL << ((ctr >> 16) & 0xFULL);
    uint64_t start = reinterpret_cast<uint64_t>(addr) & ~(lineSize - 1ULL);
    uint64_t end = reinterpret_cast<uint64_t>(addr) + size;

    for (uint64_t line = start; line < end; line += lineSize) {
      asm volatile("dc cvac, %0" :: "r"(line) : "memory");
    }
    asm volatile("dsb ishst" ::: "memory");
#else
    (void)addr;
    (void)size;
#endif
  }

  uint64_t* allocTable() {
    uint64_t pa = pmm::allocPages(0);
    if (pa == 0) {
      Uart::println("[PageTable] Failed to allocate page table");
      for (;;) asm volatile("wfe");
    }

    uint64_t* table = reinterpret_cast<uint64_t*>(pa);
    memset(table, 0, PAGE_SIZE);
    cleanDataCacheRange(table, PAGE_SIZE);
    return table;
  }

  uint64_t* walk(uint64_t* root, uint64_t addr, const WalkConfig& cfg) {
    static constexpr uint32_t levelShift[] = {39, 30, 21, 12};

    uint64_t* table = root;

    for (uint32_t level = cfg.startLevel; level < 2; ++level) {
      uint64_t mask = (level == cfg.startLevel) ? cfg.rootIndexMask : 0x1FFULL;
      uint64_t idx  = (addr >> levelShift[level]) & mask;

      if (!pte_is_valid(table[idx])) {
        if (!cfg.allocOnMiss)
          return nullptr;

        uint64_t* next = allocTable();
        table[idx] = (uint64_t)(uintptr_t)next | PTE_VALID | PTE_TABLE;
        cleanDataCacheRange(&table[idx], sizeof(table[idx]));
      }

      table = pte_next_table(table[idx]);
    }

    return &table[L2_INDEX(addr)];
  }
}
