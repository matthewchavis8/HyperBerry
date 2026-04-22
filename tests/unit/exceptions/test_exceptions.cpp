/** @file test_exceptions.cpp @brief Unit tests for EsrEc extraction and ExceptionContext layout. */

#include <gtest/gtest.h>

#include "core/exceptions/exceptions.h"

TEST(EsrEc, HvcAarch64) {
  EXPECT_EQ(getEsrEc(0x16ULL << 26), EsrEc::HvcAarch64);
}

TEST(EsrEc, SmcAarch64) {
  EXPECT_EQ(getEsrEc(0x17ULL << 26), EsrEc::SmcAarch64);
}

TEST(EsrEc, DataAbortLower) {
  EXPECT_EQ(getEsrEc(0x24ULL << 26), EsrEc::DataAbortLower);
}

TEST(EsrEc, Unknown) {
  EXPECT_EQ(getEsrEc(0), EsrEc::Unknown);
}

TEST(EsrEc, MasksCorrectly) {
  EXPECT_EQ(getEsrEc((0x3FULL << 26) | 0xFFFF), EsrEc(0x3F)); // 0x3F = max 6-bit EC value (EC field is bits [31:26])
}

TEST(EsrEc, LowBitsDoNotAffectEc) {
  EXPECT_EQ(getEsrEc((0x16ULL << 26) | 0xABCDULL), getEsrEc(0x16ULL << 26));
}

TEST(ExceptionContext, GprAtOffset0) {
  EXPECT_EQ(offsetof(ExceptionContext, gpr), 0ULL);
}

TEST(ExceptionContext, ElrAt31Times8) {
  EXPECT_EQ(offsetof(ExceptionContext, elr), 31 * 8ULL);
}

TEST(ExceptionContext, SpsrAt32Times8) {
  EXPECT_EQ(offsetof(ExceptionContext, spsr), 32 * 8ULL);
}

TEST(ExceptionContext, SizeIs33Times8) {
  EXPECT_EQ(sizeof(ExceptionContext), 33 * 8ULL);
}
