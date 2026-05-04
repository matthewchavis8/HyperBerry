/**
 * @file guestMmu.cpp
 * @brief Stage-2 MMU bring-up and mapping.
 * @ingroup mmu
 */

#include "bsp/bsp.h"
#include "core/mm/pageTable/pageTable.h"
#include "drivers/uart/uart.h"
#include "guestMmu.h"

namespace {
  constexpr uint32_t kStage2StartLevel = 1;   // 40-bit IPA with 4 KiB granules starts at L1.
  constexpr uint64_t kStage2T0sz       = 24;  // 64 - 24 = 40-bit IPA space for high RPI5 MMIO.

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

  uint64_t buildStage2PageDescriptor(uint64_t pa, bool isDevice) {
    return buildStage2BlockDescriptor(pa, isDevice) | PTE_TABLE;
  }

  uint64_t* walkL3(uint64_t* root, uint64_t ipa) {
    uint64_t* l2 = PageTable::walk(root, ipa, kStage2Walk);
    if (l2 == nullptr)
      return nullptr;

    if (!pte_is_valid(*l2)) {
      uint64_t* l3 = PageTable::allocTable();
      *l2 = reinterpret_cast<uint64_t>(l3) | PTE_VALID | PTE_TABLE;
    }

    if (!pte_is_table(*l2))
      return nullptr;

    uint64_t* l3 = pte_next_table(*l2);
    return &l3[L3_INDEX(ipa)];
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

  Uart::println("[GuestMmu] Mapping guest MMIO");
  for (size_t rangeIndex {}; rangeIndex < b::GUEST_MMIO_COUNT; ++rangeIndex) {
    const b::MmioRange& range = b::GUEST_MMIO[rangeIndex];
    for (uint64_t off {}; off < range.size; off += SIZE_2MB) {
      mapBlock(range.ipa + off, range.pa + off, true);
    }
  }

  Uart::println("[GuestMmu] Mapping guest page MMIO");
  for (size_t rangeIndex {}; rangeIndex < b::GUEST_MMIO_PAGE_COUNT; ++rangeIndex) {
    const b::MmioRange& range = b::GUEST_MMIO_PAGES[rangeIndex];
    for (uint64_t off {}; off < range.size; off += SIZE_4KB) {
      mapPage(range.ipa + off, range.pa + off, true);
    }
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

void GuestMmu::mapPage(uint64_t ipa, uint64_t pa, bool isDevice) {
  uint64_t* pte = walkL3(m_rootTable, ipa);
  if (!pte) {
    Uart::println("[ERROR] GuestMmu::mapPage walk failed");
    return;
  }
  *pte = buildStage2PageDescriptor(pa, isDevice);
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
