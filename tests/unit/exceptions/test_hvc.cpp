/** @file test_hvc.cpp @brief Unit tests for AArch64 HVC dispatch. */

#include <gtest/gtest.h>

#include "core/exceptions/hvc/hvc.h"

namespace {
constexpr uint64_t PSCI_VERSION      = 0x84000000ULL;
constexpr uint64_t PSCI_SYSTEM_OFF   = 0x84000008ULL;
constexpr uint64_t PSCI_SYSTEM_RESET = 0x84000009ULL;
constexpr uint64_t PSCI_FEATURES     = 0x8400000AULL;

constexpr uint64_t PSCI_VERSION_1_0   = 0x00010000ULL;
constexpr uint64_t PSCI_SUCCESS       = 0ULL;
constexpr uint64_t PSCI_NOT_SUPPORTED = static_cast<uint64_t>(-1);
}

TEST(HvcAarch64, PsciVersionReturnsVersion) {
  ExceptionContext regs = {};
  regs[0] = PSCI_VERSION;

  EXPECT_EQ(handleHvcAarch64(regs), HvcResult::Handled);
  EXPECT_EQ(regs[0], PSCI_VERSION_1_0);
}

TEST(HvcAarch64, PsciFeaturesReturnsSuccessForSupportedCall) {
  ExceptionContext regs = {};
  regs[0] = PSCI_FEATURES;
  regs[1] = PSCI_VERSION;

  EXPECT_EQ(handleHvcAarch64(regs), HvcResult::Handled);
  EXPECT_EQ(regs[0], PSCI_SUCCESS);
}

TEST(HvcAarch64, PsciFeaturesReturnsNotSupportedForUnknownCall) {
  ExceptionContext regs = {};
  regs[0] = PSCI_FEATURES;
  regs[1] = 0xDEADBEEFULL;

  EXPECT_EQ(handleHvcAarch64(regs), HvcResult::Handled);
  EXPECT_EQ(regs[0], PSCI_NOT_SUPPORTED);
}

TEST(HvcAarch64, PsciSystemOffReturnsHalt) {
  ExceptionContext regs = {};
  regs[0] = PSCI_SYSTEM_OFF;

  EXPECT_EQ(handleHvcAarch64(regs), HvcResult::Halt);
}

TEST(HvcAarch64, PsciSystemResetReturnsReset) {
  ExceptionContext regs = {};
  regs[0] = PSCI_SYSTEM_RESET;

  EXPECT_EQ(handleHvcAarch64(regs), HvcResult::Reset);
}

TEST(HvcAarch64, UnknownCallReturnsUnhandledAndNotSupported) {
  ExceptionContext regs = {};
  regs[0] = 0xC0FFEEULL;
  regs[1] = 0x1234ULL;

  EXPECT_EQ(handleHvcAarch64(regs), HvcResult::Unhandled);
  EXPECT_EQ(regs[0], 0xC0FFEEULL);
  EXPECT_EQ(regs[1], 0x1234ULL);
}
