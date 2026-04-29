/**
 * @file guestMmu.cpp
 * @brief Stage-2 MMU bring-up and mapping.
 * @ingroup mm
 */

#include "core/mm/pageTable/pageTable.h"
#include "drivers/uart/uart.h"
#include "guestMmu.h"

namespace {
  constexpr uint32_t kStage2StartLevel = 1;   // T0SZ=32 -> start at L1 since the guest is 32 bit for now
  constexpr uint64_t kStage2T0sz       = 32;

  constexpr PageTable::WalkConfig kStage2Walk {
    kStage2StartLevel,
    true,
  };

  uint64_t buildStage2BlockDescriptor(uint64_t pa, bool isDevice) {
    // Check if this is device memory or normal memory
    uint64_t memAttr = isDevice ? S2PTE_MEMATTR_DEVICE_nGnRnE
                                : S2PTE_MEMATTR_NORMAL_WB;
    uint64_t xn = isDevice ? S2PTE_XN_ALL : S2PTE_XN_NONE;

    return (pa & PTE_ADDR_MASK)
         | PTE_VALID
         | PTE_BLOCK
         | PTE_AF
         | S2PTE_SH_INNER
         | S2PTE_S2AP_RW
         | memAttr
         | xn;
  }
}

void GuestMmu::init(uint64_t ipaBase, uint64_t hostPaBase, uint64_t sizeBytes) {
  Uart::println("[GuestMmu] init called");
  m_rootTable = PageTable::allocTable();
  Uart::println("[GuestMmu] root table={}", m_rootTable);

  uint64_t vtcr = VTCR_T0SZ(kStage2T0sz)
                | VTCR_SL0_L1
                | VTCR_TG0_4K
                | VTCR_SH0_IS
                | VTCR_ORGN0_WB
                | VTCR_IRGN0_WB
                | VTCR_PS_40BIT
                | VTCR_RES1;

  Uart::println("[GuestMmu] Programming VTCR_EL2");
  asm volatile("msr vtcr_el2, %0" :: "r"(vtcr) : "memory");
  asm volatile("isb");

  Uart::println("[GuestMmu] Mapping guest IPA range");
  for (uint64_t off {}; off < sizeBytes; off += SIZE_2MB) {
    mapBlock(ipaBase + off, hostPaBase + off, false);
  }

  Uart::println("[GuestMmu] init finished");
}

void GuestMmu::mapBlock(uint64_t ipa, uint64_t pa, bool isDevice) {
  uint64_t* pte = PageTable::walk(m_rootTable, ipa, kStage2Walk);
  if (!pte) {
    Uart::println("[ERROR] GuestMmu::mapBlock walk failed");
    return;
  }
  *pte = buildStage2BlockDescriptor(pa, isDevice);
}

void GuestMmu::enable(uint8_t vmid) {
  m_vmid = vmid;

  uint64_t rootPa = (uint64_t)(uintptr_t)m_rootTable;
  uint64_t vttbr  = ((uint64_t)vmid << 48) | (rootPa & PTE_ADDR_MASK);
  Uart::println("[GuestMmu] Programming VTTBR_EL2");
  asm volatile("msr vttbr_el2, %0" :: "r"(vttbr) : "memory");
  asm volatile("isb");

  Uart::println("[GuestMmu] Setting HCR_EL2.VM");
  uint64_t hcr;
  asm volatile("mrs %0, hcr_el2" : "=r"(hcr));
  hcr |= (1ULL << 0);
  asm volatile("msr hcr_el2, %0" :: "r"(hcr) : "memory");
  asm volatile("isb");

  tlbFlushAllGuest();

  Uart::println("[GuestMmu] stage-2 enabled");
}

void GuestMmu::tlbFlushAllGuest() {
  asm volatile("tlbi vmalls12e1is" ::: "memory");
  asm volatile("dsb ish" ::: "memory");
  asm volatile("isb");
}
