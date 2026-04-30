/**
 * @file hostMmu.cpp
 * @brief EL2 stage-1 MMU bring-up and mapping operations.
 * @ingroup mmu
 */

#include "core/mm/pageTable/pageTable.h"
#include "drivers/uart/uart.h"
#include "hostMmu.h"

namespace {
  uint64_t* l0_table;

  constexpr PageTable::WalkConfig kStage1Walk {
    0,
    true,
  };

  constexpr PageTable::WalkConfig kStage1Lookup {
    0,
    false,
  };
}

namespace HostMmu {
  void init() {
    Uart::println("[HostMmu] init called");
    l0_table = PageTable::allocTable();
    Uart::println("[HostMmu] L0 table={}", l0_table);

    Uart::println("[HostMmu] Programming MAIR");
    uint64_t mair = (0xFFULL << (MAIR_IDX_NORMAL * 8))     // Normal memory
                  | (0x00ULL << (MAIR_IDX_DEVICE * 8))     // Device memory
                  | (0x44ULL << (MAIR_IDX_NORMAL_NC * 8)); // Normal but non cacheable memory
    asm volatile("msr mair_el2, %0" :: "r"(mair) : "memory");

    Uart::println("[HostMmu] Programming TCR");
    uint64_t tcr = (16ULL << 0)
                 | (0ULL << 14)
                 | (1ULL << 8)
                 | (1ULL << 10)
                 | (3ULL << 12)
                 | (2ULL << 16);
    asm volatile("msr tcr_el2, %0" :: "r"(tcr) : "memory");
    asm volatile("isb");

    Uart::println("[HostMmu] Mapping HV DRAM");
    mapRange(HV_VA_BASE, HV_VA_BASE, HV_VA_SIZE, PTE_NORMAL | PTE_AP_RW);

    Uart::println("[HostMmu] Mapping HV MMIO space");
    mapRange(b::HvMmioBase,
             b::HvMmioBase,
             b::HvMmioSize,
             PTE_DEVICE);

    Uart::println("[HostMmu] Programming TTBR0");
    asm volatile("msr ttbr0_el2, %0" :: "r"((uint64_t)(uintptr_t)l0_table) : "memory");
    asm volatile("dsb ishst" ::: "memory");
    asm volatile("isb");

    Uart::println("[HostMmu] Enabling SCTLR.M/C/I");
    uint64_t sctlr;
    asm volatile("mrs %0, sctlr_el2" : "=r"(sctlr));
    sctlr |= (1ULL << 0)   // enable MMU
          |  (1ULL << 2)   // enable data cache
          |  (1ULL << 12); // enable instruction cache
    asm volatile("msr sctlr_el2, %0" :: "r"(sctlr) : "memory");
    asm volatile("isb");

    Uart::println("[HostMmu] init finished");
  }

  void mapRange(uint64_t va, uint64_t pa, uint64_t size, uint64_t flags) {
    for (uint64_t off {}; off < size; off += SIZE_2MB) {
      uint64_t* pte = PageTable::walk(l0_table, va + off, kStage1Walk);
      if (!pte) {
        Uart::println("[ERROR] HostMmu::mapRange walk failed");
        break;
      }
      *pte = (((pa + off) & PTE_ADDR_MASK) | flags | PTE_BLOCK);
    }
  }

  void unmapRange(uint64_t va, uint64_t size) {
    for (uint64_t off {}; off < size; off += SIZE_2MB) {
      uint64_t* pte = PageTable::walk(l0_table, va + off, kStage1Lookup);
      if (!pte) {
        Uart::println("[ERROR] HostMmu::unmapRange walk failed");
        break;
      }
      *pte = 0;
      tlbFlushVa(va + off);
    }
  }

  void tlbFlushAll() {
    asm volatile("tlbi alle2is" ::: "memory");
    asm volatile("dsb sy" ::: "memory");
    asm volatile("isb");
  }

  void tlbFlushVa(uint64_t va) {
    asm volatile("tlbi vae2is, %0" :: "r"(va >> 12) : "memory");
    asm volatile("dsb sy" ::: "memory");
    asm volatile("isb");
  }
}
