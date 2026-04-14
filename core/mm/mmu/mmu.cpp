/**
 * @file mmu.cpp
 * @brief EL2 Stage-1 MMU bring-up and mapping operations.
 * @ingroup mm
 */

#include "core/mm/pmm/pmm.h"
#include "drivers/uart/uart.h"
#include "lib/strings/strings.h"
#include "mmu.h"

namespace {
  uint64_t* l0_table;

  uint64_t* allocTable() {
    Uart::println("[MMU] allocTable: request");
    uint64_t pa = pmm::allocPages(0);
    Uart::println("[MMU] allocTable: pa=");
    Uart::writeHex(pa);
    Uart::println("");
    if (pa == 0) {
      Uart::println("[MMU] Failed to allocate page table");
      for (;;) asm volatile("wfe");
    }
    uint64_t* table = reinterpret_cast<uint64_t*>(pa);
    memset(table, 0, PAGE_SIZE);
    Uart::println("[MMU] allocTable: zeroed");
    return table;
  }

  uint64_t* walk(uint64_t va, int alloc) {
    // Walk L0
    uint64_t l0_idx = L0_INDEX(va);

    // If miss then remap
    if (!pte_is_valid(l0_table[l0_idx])) {
      if (!alloc)
        return nullptr;

      uint64_t* l1 = allocTable();
      l0_table[l0_idx] = (uint64_t)(uintptr_t)l1 | PTE_VALID | PTE_TABLE;
    }

    // Walk L1
    uint64_t* l1_table = pte_next_table(l0_table[l0_idx]); // Base address of the next table
    uint64_t l1_idx = L1_INDEX(va);                        // Table entry
    // if miss then remap again
    if (!pte_is_valid(l1_table[l1_idx])) {
      if (!alloc)
        return nullptr;

      uint64_t* l2 = allocTable();
      l1_table[l1_idx] = (uint64_t)(uintptr_t)l2 | PTE_VALID | PTE_TABLE;
    }

    // Walk L2
    uint64_t* l2_table = pte_next_table(l1_table[l1_idx]);

    // return the entry 
    return &l2_table[L2_INDEX(va)];
  }
}

namespace mmu {
  void init() {
    Uart::println("[MMU] init called");
    l0_table = allocTable();
    Uart::println("[MMU] L0 table=");
    Uart::writeHex((uint64_t)(uintptr_t)l0_table);
    Uart::println("");

    // Memory attributes
    Uart::println("[MMU] Programming MAIR");
    uint64_t mair = (0xFFULL << (MAIR_IDX_NORMAL * 8))     // Normal memory
                  | (0x00ULL << (MAIR_IDX_DEVICE * 8))     // Device memory
                  | (0x44ULL << (MAIR_IDX_NORMAL_NC * 8)); // Normal but non cacheable memory
    asm volatile("msr mair_el2, %0" :: "r"(mair) : "memory");

    // Translation control
    Uart::println("[MMU] Programming TCR");
    uint64_t tcr = (16ULL << 0)
                 | (0ULL << 14)
                 | (1ULL << 8)
                 | (1ULL << 10)
                 | (3ULL << 12)
                 | (2ULL << 16);
    asm volatile("msr tcr_el2, %0" :: "r"(tcr) : "memory");
    asm volatile("isb");

    // Map Hypervisor DRAM
    Uart::println("[MMU] Mapping HV DRAM");
    mapRange(HV_VA_BASE,  HV_VA_BASE, HV_VA_SIZE, PTE_NORMAL | PTE_AP_RW);
    // Map RPI5 device space
    Uart::println("[MMU] Mapping periph space");
    mapRange(BCM2712_PERIPH_BASE, BCM2712_PERIPH_BASE, BCM2712_PERIPH_SIZE, PTE_DEVICE);
    
    // Bring up table
    Uart::println("[MMU] Programming TTBR0");
    asm volatile("msr ttbr0_el2, %0" :: "r"((uint64_t)(uintptr_t)l0_table) : "memory");
    asm volatile("dsb ishst" ::: "memory");
    asm volatile("isb");
   
    // Enable MMU 
    Uart::println("[MMU] Enabling SCTLR.M/C/I");
    uint64_t sctlr;
    asm volatile("mrs %0, sctlr_el2" : "=r"(sctlr));
    sctlr |= (1ULL << 0)   // enable MMU
          |  (1ULL << 2)   // enable data cache
          |  (1ULL << 12); // enable instruction cache
    asm volatile("msr sctlr_el2, %0" :: "r"(sctlr) : "memory");
    asm volatile("isb");
    
    Uart::println("[MMU] mmu init finished");
  }

  void mapRange(uint64_t va, uint64_t pa, uint64_t size, uint64_t flags) {
    for (uint64_t off {}; off < size; off += SIZE_2MB) {
      uint64_t* pte = walk(va + off, 1);
      if (!pte) {
        Uart::println("[ERROR] Failed to mapRange");
        break;
      }
      *pte = (((pa + off) & PTE_ADDR_MASK) | flags | PTE_BLOCK);
    }
  }
  
  void unmapRange(uint64_t va, uint64_t size) {
    for (uint64_t off {}; off < size; off += SIZE_2MB) {
      uint64_t* pte = walk(va + off, 0);
      if (!pte) {
        Uart::println("[ERROR] Failed to unmapRange");
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
