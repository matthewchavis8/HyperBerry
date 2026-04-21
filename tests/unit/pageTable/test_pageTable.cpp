/**
 * @file test_pageTable.cpp
 * @brief Unit tests for shared page-table bit helpers and index macros.
 */

#include <gtest/gtest.h>

#include "core/mm/pageTable/pageTable.h"

TEST(PageTable, ComputesTableIndicesFromAddress) {
  constexpr uint64_t kAddr = 0x0000123456789ABCULL;

  EXPECT_EQ(L0_INDEX(kAddr), 0x24ULL);
  EXPECT_EQ(L1_INDEX(kAddr), 0xD1ULL);
  EXPECT_EQ(L2_INDEX(kAddr), 0xB3ULL);
  EXPECT_EQ(L3_INDEX(kAddr), 0x189ULL);
}

TEST(PageTable, ExtractsNextTableAddressFromDescriptor) {
  constexpr uint64_t kTableAddr = 0x0000000012345000ULL;
  uint64_t* next = pte_next_table(kTableAddr | PTE_VALID | PTE_TABLE);

  EXPECT_EQ(reinterpret_cast<uintptr_t>(next), kTableAddr);
}

TEST(PageTable, DetectsValidTableAndBlockDescriptors) {
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

TEST(PageTable, AddressMaskCoversBits47Through12) {
  EXPECT_EQ(PTE_ADDR_MASK, 0x0000FFFFFFFFF000ULL);
}

TEST(PageTable, BlockSizeConstantsMatch4KbGranuleLevels) {
  EXPECT_EQ(SIZE_4KB, 0x1000ULL);
  EXPECT_EQ(SIZE_2MB, 0x200000ULL);
  EXPECT_EQ(SIZE_1GB, 0x40000000ULL);
}
