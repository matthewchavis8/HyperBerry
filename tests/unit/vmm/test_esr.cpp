// @file test_esr.cpp
// @brief Unit tests for ESR_EL2 decode and the ExceptionContext layout.

#include <gtest/gtest.h>

#include "core/vmm/esr.h"

TEST(EsrEc, HvcAarch64) {
    EXPECT_EQ(getEsrEc(0x16ULL << 26), EsrEc::HVC_AARCH64);
}

TEST(EsrEc, SmcAarch64) {
    EXPECT_EQ(getEsrEc(0x17ULL << 26), EsrEc::SMC_AARCH64);
}

TEST(EsrEc, DataAbortLower) {
    EXPECT_EQ(getEsrEc(0x24ULL << 26), EsrEc::DATA_ABORT_LOWER);
}

TEST(EsrEc, Unknown) {
    EXPECT_EQ(getEsrEc(0), EsrEc::UNKNOWN);
}

TEST(EsrEc, MasksCorrectly) {
    EXPECT_EQ(getEsrEc((0x3FULL << 26) | 0xFFFF),
            EsrEc(0x3F)); // 0x3F = max 6-bit EC value (EC field is bits [31:26])
}

TEST(EsrEc, LowBitsDoNotAffectEc) {
    EXPECT_EQ(getEsrEc((0x16ULL << 26) | 0xABCDULL), getEsrEc(0x16ULL << 26));
}

TEST(EsrIss, KeepsBits24To0) {
    EXPECT_EQ(getEsrIss(0xFFFFFFFFULL), 0x1FFFFFFU);
}

TEST(EsrIss, IgnoresEcAndIl) {
    EXPECT_EQ(getEsrIss((0x24ULL << 26) | (1ULL << 25) | 0x93ULL), 0x93U);
}

TEST(ExceptionContext, Stores31GeneralPurposeRegisters) {
    ExceptionContext ctx {};

    EXPECT_EQ(ctx.size(), 31ULL);
}

TEST(ExceptionContext, SizeIs31Times8) {
    EXPECT_EQ(sizeof(ExceptionContext), 31 * 8ULL);
}
