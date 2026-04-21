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
  uint64_t* allocTable() {
    Uart::println("[PageTable] allocTable: request");
    uint64_t pa = pmm::allocPages(0);

    Uart::println("[PageTable] allocTable: pa=");
    Uart::writeHex(pa);
    Uart::println("");

    if (pa == 0) {
      Uart::println("[PageTable] Failed to allocate page table");
      for (;;) asm volatile("wfe");
    }

    uint64_t* table = reinterpret_cast<uint64_t*>(pa);
    memset(table, 0, PAGE_SIZE);
    return table;
  }

  uint64_t* walk(uint64_t* root, uint64_t addr, const WalkConfig& cfg) {
    static constexpr uint32_t levelShift[] = {39, 30, 21, 12};

    uint64_t* table = root;

    for (uint32_t level = cfg.startLevel; level < 2; ++level) {
      uint64_t idx = (addr >> levelShift[level]) & 0x1FFULL;

      if (!pte_is_valid(table[idx])) {
        if (!cfg.allocOnMiss)
          return nullptr;

        uint64_t* next = allocTable();
        table[idx] = (uint64_t)(uintptr_t)next | PTE_VALID | PTE_TABLE;
      }

      table = pte_next_table(table[idx]);
    }

    return &table[L2_INDEX(addr)];
  }
}
