/**
 * @file test_guestMmu.cpp
 * @brief Unit tests for stage-2 descriptor bits and VTCR field builders.
 */

#include <gtest/gtest.h>

#include "core/mm/mmu/guestMmu/guestMmu.h"

TEST(GuestMmu, StageTwoS2apEncodingMatchesArmSpec) {
  EXPECT_EQ(S2PTE_S2AP_NONE, 0b00ULL << 6);
  EXPECT_EQ(S2PTE_S2AP_RO,   0b01ULL << 6);
  EXPECT_EQ(S2PTE_S2AP_WO,   0b10ULL << 6);
  EXPECT_EQ(S2PTE_S2AP_RW,   0b11ULL << 6);
}

TEST(GuestMmu, MemAttrOccupiesBits5Through2) {
  EXPECT_EQ(S2PTE_MEMATTR_DEVICE_nGnRnE, 0x0ULL << 2);
  EXPECT_EQ(S2PTE_MEMATTR_NORMAL_WB,     0xFULL << 2);
}

TEST(GuestMmu, XnSitsInBits54Through53) {
  EXPECT_EQ(S2PTE_XN_NONE, 0ULL);
  EXPECT_EQ(S2PTE_XN_ALL,  2ULL << 53);
}

TEST(GuestMmu, Vtcr32BitIpaMatchesT0szMask) {
  EXPECT_EQ(VTCR_T0SZ(32), 32ULL);
  EXPECT_EQ(VTCR_T0SZ(64), 0ULL);  // masked to 6 bits.
}

TEST(GuestMmu, VtcrStage2FieldBuildersDoNotOverlap) {
  uint64_t composed = VTCR_SL0_L1
                    | VTCR_TG0_4K
                    | VTCR_SH0_IS
                    | VTCR_ORGN0_WB
                    | VTCR_IRGN0_WB
                    | VTCR_PS_40BIT
                    | VTCR_RES1;

  // Bit 31 is RES1, SL0 is [7:6], TG0 is [15:14], SH0 is [13:12],
  // ORGN0 is [11:10], IRGN0 is [9:8], PS is [18:16].
  EXPECT_TRUE(composed & (1ULL << 31));
  EXPECT_EQ((composed >> 6)  & 0x3,  0b01ULL);
  EXPECT_EQ((composed >> 14) & 0x3,  0b00ULL);
  EXPECT_EQ((composed >> 12) & 0x3,  0b11ULL);
  EXPECT_EQ((composed >> 10) & 0x3,  0b01ULL);
  EXPECT_EQ((composed >> 8)  & 0x3,  0b01ULL);
  EXPECT_EQ((composed >> 16) & 0x7,  0b010ULL);
}
