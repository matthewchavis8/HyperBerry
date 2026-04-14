/**
 * @file test_mmu.cpp
 * @brief Unit tests for MMU descriptor and index helpers.
 */

#include <gtest/gtest.h>

#include "core/mm/mmu/mmu.h"

TEST(Mmu, ComputesTableIndicesFromVirtualAddress) {
  constexpr uint64_t kVa = 0x0000123456789ABCULL;

  EXPECT_EQ(L0_INDEX(kVa), 0x24ULL);
  EXPECT_EQ(L1_INDEX(kVa), 0xD1ULL);
  EXPECT_EQ(L2_INDEX(kVa), 0xB3ULL);
  EXPECT_EQ(L3_INDEX(kVa), 0x189ULL);
}

TEST(Mmu, ExtractsNextTableAddressFromDescriptor) {
  constexpr uint64_t kTableAddr = 0x0000000012345000ULL;
  uint64_t* next = pte_next_table(kTableAddr | PTE_VALID | PTE_TABLE);

  EXPECT_EQ(reinterpret_cast<uintptr_t>(next), kTableAddr);
}

TEST(Mmu, DetectsValidTableAndBlockDescriptors) {
  constexpr uint64_t kTableEntry = 0x0000000045678000ULL | PTE_VALID | PTE_TABLE;
  constexpr uint64_t kBlockEntry = 0x0000000089ABC000ULL | PTE_VALID | PTE_AF;

  EXPECT_TRUE(pte_is_valid(kTableEntry));
  EXPECT_TRUE(pte_is_table(kTableEntry));
  EXPECT_FALSE(pte_is_block(kTableEntry));

  EXPECT_TRUE(pte_is_valid(kBlockEntry));
  EXPECT_FALSE(pte_is_table(kBlockEntry));
  EXPECT_TRUE(pte_is_block(kBlockEntry));

  EXPECT_FALSE(pte_is_valid(0));
}

TEST(Mmu, BuildsExpectedNormalAndDeviceFlags) {
  EXPECT_EQ(PTE_NORMAL, PTE_VALID | PTE_AF | PTE_SH_INNER | PTE_ATTRIDX(MAIR_IDX_NORMAL));
  EXPECT_EQ(PTE_DEVICE, PTE_VALID | PTE_AF | PTE_SH_NONE | PTE_ATTRIDX(MAIR_IDX_DEVICE) | PTE_XN);
}
