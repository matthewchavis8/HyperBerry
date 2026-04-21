/**
 * @file test_hostMmu.cpp
 * @brief Unit tests for stage-1 (EL2 self-map) attribute presets.
 */

#include <gtest/gtest.h>

#include "core/mm/mmu/hostMmu/hostMmu.h"

TEST(HostMmu, BuildsExpectedNormalAndDeviceFlags) {
  EXPECT_EQ(PTE_NORMAL,
            PTE_VALID | PTE_AF | PTE_SH_INNER | PTE_ATTRIDX(MAIR_IDX_NORMAL));
  EXPECT_EQ(PTE_DEVICE,
            PTE_VALID | PTE_AF | PTE_SH_NONE | PTE_ATTRIDX(MAIR_IDX_DEVICE) | PTE_XN);
}

TEST(HostMmu, MairIndicesAreDistinctAndInRange) {
  EXPECT_NE(MAIR_IDX_NORMAL, MAIR_IDX_DEVICE);
  EXPECT_NE(MAIR_IDX_DEVICE, MAIR_IDX_NORMAL_NC);
  EXPECT_LT(MAIR_IDX_NORMAL_NC, 8);  // MAIR_EL2 has 8 slots.
}

TEST(HostMmu, AttrIdxBuilderShiftsIntoBits4Through2) {
  EXPECT_EQ(PTE_ATTRIDX(0), 0ULL);
  EXPECT_EQ(PTE_ATTRIDX(1), (1ULL << 2));
  EXPECT_EQ(PTE_ATTRIDX(7), (7ULL << 2));
}
