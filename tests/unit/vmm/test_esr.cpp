// @file test_esr.cpp
// @brief Unit tests for ESR_EL2 decode and the EL2 frame layout.

#include <gtest/gtest.h>

#include "core/vmm/esr/esr.h"
#include "core/vmm/exceptions/exceptions.h"

TEST(EsrEc, HvcAarch64) {
    EXPECT_EQ(GetEsrEc(0x16ULL << 26), EsrEc::HVC_AARCH64);
}

TEST(EsrEc, SmcAarch64) {
    EXPECT_EQ(GetEsrEc(0x17ULL << 26), EsrEc::SMC_AARCH64);
}

TEST(EsrEc, DataAbortLower) {
    EXPECT_EQ(GetEsrEc(0x24ULL << 26), EsrEc::DATA_ABORT_LOWER);
}

TEST(EsrEc, Unknown) {
    EXPECT_EQ(GetEsrEc(0), EsrEc::UNKNOWN);
}

TEST(EsrEc, MasksCorrectly) {
    EXPECT_EQ(GetEsrEc((0x3FULL << 26) | 0xFFFF),
            EsrEc(0x3F)); // 0x3F = max 6-bit EC value (EC field is bits [31:26])
}

TEST(EsrEc, LowBitsDoNotAffectEc) {
    EXPECT_EQ(GetEsrEc((0x16ULL << 26) | 0xABCDULL), GetEsrEc(0x16ULL << 26));
}

TEST(EsrIss, KeepsBits24To0) {
    EXPECT_EQ(GetEsrIss(0xFFFFFFFFULL), 0x1FFFFFFU);
}

TEST(EsrIss, IgnoresEcAndIl) {
    EXPECT_EQ(GetEsrIss((0x24ULL << 26) | (1ULL << 25) | 0x93ULL), 0x93U);
}

TEST(El2ExceptionFrame, Stores31GeneralPurposeRegisters) {
    El2ExceptionFrame ctx {};

    EXPECT_EQ(ctx.x.size(), 31ULL);
}

TEST(El2ExceptionFrame, HasAlignedAssemblyLayout) {
    EXPECT_EQ(sizeof(El2ExceptionFrame), 272ULL);
    EXPECT_EQ(offsetof(El2ExceptionFrame, elr), 248ULL);
}
