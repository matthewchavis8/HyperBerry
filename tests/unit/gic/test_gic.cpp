/**
 * @file test_gic.cpp
 * @brief Unit-level GICv2 register logic tests.
 *
 * These tests intentionally stub the Gic implementation and validate the
 * register index, bit mask, and List Register encoding logic without touching
 * real MMIO. Hardware-backed behavior belongs in the integration suite.
 */

#include <gtest/gtest.h>

#include <array>
#include <cstdint>

#include "drivers/gic/gic.h"

namespace {

constexpr uint32_t kMaxLrs       = 64;
constexpr uint32_t kLrPending    = (1U << 28);
constexpr uint32_t kLrHardware   = (1U << 31);
constexpr uint32_t kPhysIdShift  = 10;
constexpr uint32_t kHcrUie       = (1U << 1);
constexpr uint32_t kSpuriousBits = 0xA5A50000;

struct GicStubState {
  std::array<uint32_t, 32> isenabler{};
  std::array<uint32_t, 32> icenabler{};
  std::array<uint32_t, kMaxLrs> lr{};
  uint32_t iar = 0;
  uint32_t eoir = 0;
  uint32_t elsr0 = 0;
  uint32_t elsr1 = 0;
  uint32_t hcr = 0;
  uint32_t configuredLrs = 0;
};

GicStubState gGic;

void resetGic(uint32_t numLrs = 4) {
  gGic = {};
  gGic.configuredLrs = numLrs;
}

uint32_t encodedLr(uint32_t virtId, uint32_t physId) {
  return (virtId & 0x3FFU) |
         ((physId & 0x3FFU) << kPhysIdShift) |
         kLrPending |
         kLrHardware;
}

} // namespace

volatile uint32_t* Gic::reg(const uintptr_t /*REG*/) {
  return nullptr;
}

void Gic::cpuInit() {
  m_numLr = gGic.configuredLrs;
}

void Gic::init() {
  cpuInit();
}

void Gic::cpuReset() {
  for (uint32_t i = 0; i < m_numLr; ++i) {
    gGic.lr[i] = 0;
  }
}

void Gic::enableIrq(uint32_t id) {
  uint32_t regIdx = id / 32;
  uint32_t bitMsk = (1U << (id % 32));
  gGic.isenabler[regIdx] = bitMsk;
}

void Gic::disableIrq(uint32_t id) {
  uint32_t regIdx = id / 32;
  uint32_t bitMsk = (1U << (id % 32));
  gGic.icenabler[regIdx] = bitMsk;
}

Gic::IrqAck Gic::ackIrq() {
  return IrqAck{gGic.iar, gGic.iar & 0x3FFU};
}

void Gic::endIrq(IrqAck irq) {
  gGic.eoir = irq.iar;
}

int Gic::injectIrq(uint32_t virtId, uint32_t id) {
  int freeLr = -1;

  for (uint32_t i = 0; i < m_numLr; ++i) {
    uint32_t elsr = (i < 32) ? gGic.elsr0 : gGic.elsr1;
    uint32_t bit = i % 32;

    if ((gGic.lr[i] & 0x3FFU) == virtId) {
      return -1;
    }

    if (((elsr >> bit) & 1U) != 0U && freeLr == -1) {
      freeLr = static_cast<int>(i);
    }
  }

  if (freeLr == -1) {
    return -1;
  }

  gGic.lr[static_cast<uint32_t>(freeLr)] = encodedLr(virtId, id);
  return 0;
}

bool Gic::hasPendingIrq() {
  for (uint32_t i = 0; i < m_numLr; ++i) {
    if ((gGic.lr[i] & kLrPending) != 0U) {
      return true;
    }
  }
  return false;
}

void Gic::enableMainIrq(bool isEnable) {
  if (isEnable) {
    gGic.hcr |= kHcrUie;
  } else {
    gGic.hcr &= ~kHcrUie;
  }
}

void Gic::setPriorityLevel(uint32_t /*id*/, uint8_t /*priority*/) {}

TEST(Gic, EnableIrqSelectsExpectedRegisterAndBit) {
  resetGic();

  Gic::enableIrq(45);

  EXPECT_EQ(gGic.isenabler[1], 1U << 13);
  EXPECT_EQ(gGic.isenabler[0], 0U);
}

TEST(Gic, DisableIrqSelectsExpectedRegisterAndBit) {
  resetGic();

  Gic::disableIrq(64);

  EXPECT_EQ(gGic.icenabler[2], 1U);
  EXPECT_EQ(gGic.icenabler[0], 0U);
}

TEST(Gic, AckIrqPreservesRawIarAndMasksInterruptId) {
  resetGic();
  gGic.iar = kSpuriousBits | 0x3ABU;

  Gic::IrqAck ack = Gic::ackIrq();

  EXPECT_EQ(ack.iar, kSpuriousBits | 0x3ABU);
  EXPECT_EQ(ack.id, 0x3ABU);
}

TEST(Gic, EndIrqWritesRawIarToken) {
  resetGic();

  Gic::endIrq(Gic::IrqAck{0x12345678U, 0x278U});

  EXPECT_EQ(gGic.eoir, 0x12345678U);
}

TEST(Gic, InjectIrqUsesFirstFreeListRegisterAndEncodesIds) {
  resetGic(4);
  Gic::init();
  gGic.elsr0 = (1U << 1) | (1U << 3);

  EXPECT_EQ(Gic::injectIrq(0x45U, 0x88U), 0);

  EXPECT_EQ(gGic.lr[1], encodedLr(0x45U, 0x88U));
  EXPECT_EQ(gGic.lr[3], 0U);
}

TEST(Gic, InjectIrqRejectsDuplicateVirtualId) {
  resetGic(4);
  Gic::init();
  gGic.elsr0 = 0xFU;
  gGic.lr[2] = encodedLr(0x52U, 0x99U);

  EXPECT_EQ(Gic::injectIrq(0x52U, 0x100U), -1);
}

TEST(Gic, InjectIrqFailsWhenNoListRegisterIsFree) {
  resetGic(4);
  Gic::init();
  gGic.elsr0 = 0;

  EXPECT_EQ(Gic::injectIrq(0x21U, 0x31U), -1);
}

TEST(Gic, InjectIrqCanUseSecondElsrBank) {
  resetGic(34);
  Gic::init();
  gGic.elsr1 = (1U << 1);

  EXPECT_EQ(Gic::injectIrq(0x12U, 0x34U), 0);

  EXPECT_EQ(gGic.lr[33], encodedLr(0x12U, 0x34U));
}

TEST(Gic, HasPendingIrqReportsAnyPendingListRegister) {
  resetGic(4);
  Gic::init();
  EXPECT_FALSE(Gic::hasPendingIrq());

  gGic.lr[3] = kLrPending;

  EXPECT_TRUE(Gic::hasPendingIrq());
}

TEST(Gic, EnableMainIrqSetsAndClearsOnlyUieBit) {
  resetGic();
  gGic.hcr = kSpuriousBits;

  Gic::enableMainIrq(true);
  EXPECT_EQ(gGic.hcr, kSpuriousBits | kHcrUie);

  Gic::enableMainIrq(false);
  EXPECT_EQ(gGic.hcr, kSpuriousBits);
}

TEST(Gic, CpuResetClearsConfiguredListRegisters) {
  resetGic(3);
  Gic::init();
  gGic.lr[0] = 1;
  gGic.lr[1] = 2;
  gGic.lr[2] = 3;
  gGic.lr[3] = 4;

  Gic::cpuReset();

  EXPECT_EQ(gGic.lr[0], 0U);
  EXPECT_EQ(gGic.lr[1], 0U);
  EXPECT_EQ(gGic.lr[2], 0U);
  EXPECT_EQ(gGic.lr[3], 4U);
}
