/**
 * @file test_timer.cpp
 * @brief Hardware-backed integration tests for the ARM generic timer driver.
 */

#include "core/exceptions/exceptions.h"
#include "bsp/bsp.h"
#include "drivers/gic/gic.h"
#include "drivers/timer/timer.h"
#include "drivers/uart/uart.h"
#include "tests/integration/suite.h"
#include <stdint.h>

namespace {

extern "C" char test_timer_vectors[];

Timer* gActiveTimer = nullptr;
volatile uint32_t gCallbackCount = 0;
volatile uint32_t gIrqCount = 0;
volatile uint32_t gLastIrqId = 0;
volatile bool gCallbackContextMatched = false;

namespace GicReg {
namespace Dist {
constexpr uintptr_t BASE = b::GIC_DISTRIBUTOR_BASE;
constexpr uintptr_t CTLR = BASE + 0x000;
constexpr uintptr_t IGROUPR = BASE + 0x080;
} // namespace Dist
} // namespace GicReg

volatile uint32_t* reg(uintptr_t addr) {
  return reinterpret_cast<volatile uint32_t*>(addr);
}

uint32_t irqBit(uint32_t id) {
  return 1U << (id % 32);
}

uintptr_t irqReg(uintptr_t base, uint32_t id) {
  return base + (id / 32) * sizeof(uint32_t);
}

void configureTimerPpiGroup0() {
  *reg(irqReg(GicReg::Dist::IGROUPR, Timer::IRQ)) =
      *reg(irqReg(GicReg::Dist::IGROUPR, Timer::IRQ)) & ~irqBit(Timer::IRQ);
}

void enableDistributorGroups() {
  *reg(GicReg::Dist::CTLR) = *reg(GicReg::Dist::CTLR) | 0x3U;
}

void spin(uint32_t iterations) {
  for (uint32_t i = 0; i < iterations; ++i) {
    asm volatile("nop");
  }
}

uint64_t saveDaif() {
  uint64_t saved = 0;
  asm volatile("mrs %0, daif" : "=r"(saved));
  return saved;
}

void restoreDaif(uint64_t saved) {
  asm volatile(
    "msr daif, %0\n"
    "isb"
    :: "r"(saved)
    : "memory"
  );
}

void unmaskIrqAndFiq() {
  asm volatile(
    "msr daifclr, #3\n"
    "isb"
    ::: "memory"
  );
}

uint64_t installTestVbar() {
  uint64_t saved = 0;
  asm volatile("mrs %0, vbar_el2" : "=r"(saved));
  asm volatile(
    "msr vbar_el2, %0\n"
    "isb"
    :: "r"(reinterpret_cast<uint64_t>(test_timer_vectors))
    : "memory"
  );
  return saved;
}

void restoreVbar(uint64_t saved) {
  asm volatile(
    "msr vbar_el2, %0\n"
    "isb"
    :: "r"(saved)
    : "memory"
  );
}

uint64_t routePhysicalInterruptsToEl2() {
  constexpr uint64_t HCR_IMO = 1ULL << 4;
  constexpr uint64_t HCR_FMO = 1ULL << 3;

  uint64_t saved = 0;
  asm volatile("mrs %0, hcr_el2" : "=r"(saved));
  asm volatile(
    "msr hcr_el2, %0\n"
    "isb"
    :: "r"(saved | HCR_IMO | HCR_FMO)
    : "memory"
  );
  return saved;
}

void restoreHcr(uint64_t saved) {
  asm volatile(
    "msr hcr_el2, %0\n"
    "isb"
    :: "r"(saved)
    : "memory"
  );
}

void timerCallback(void* ctx) {
  gCallbackContextMatched = ctx == reinterpret_cast<void*>(0x54494D45ULL);
  ++gCallbackCount;
}

uint64_t oneMillisecondTicks(const Timer& timer) {
  uint64_t ticks = timer.getFrequency() / 1000U;
  return ticks == 0 ? 1 : ticks;
}

bool waitForTimerCallback() {
  for (uint32_t i = 0; i < 10000000; ++i) {
    if (gCallbackCount != 0) {
      return true;
    }
    asm volatile("wfe");
  }
  return false;
}

} // namespace

extern "C" void handle_test_timer_el2_irq(ExceptionContext* ctx) {
  (void)ctx;

  Gic::IrqAck ack = Gic::ackIrq();
  gLastIrqId = ack.id;

  if (ack.id == Timer::IRQ && gActiveTimer != nullptr) {
    ++gIrqCount;
    gActiveTimer->handleIrq();
  }

  Gic::endIrq(ack);
}

static bool test_frequency_and_counter_progress() {
  Timer timer;
  timer.init();

  uint64_t frequency = timer.getFrequency();
  uint64_t first = timer.getRawCount();
  spin(1000);
  uint64_t second = timer.getRawCount();

  return frequency != 0 && second > first;
}

static bool test_elapsed_ticks_reset_on_start() {
  Timer timer;
  timer.init();
  timer.setIntervalTicks(oneMillisecondTicks(timer));

  timer.start();
  uint64_t immediate = timer.getElapsedTicks();
  spin(1000);
  uint64_t later = timer.getElapsedTicks();
  timer.stop();

  return later > immediate;
}

static bool test_handle_irq_reloads_and_invokes_callback() {
  Timer timer;
  timer.init();
  timer.setIntervalTicks(oneMillisecondTicks(timer));

  gCallbackCount = 0;
  gCallbackContextMatched = false;
  timer.setCallback(timerCallback, reinterpret_cast<void*>(0x54494D45ULL));

  timer.handleIrq();
  timer.handleIrq();

  return gCallbackCount == 2 && gCallbackContextMatched;
}

static bool test_physical_timer_irq_invokes_callback() {
  Timer timer;
  timer.init();
  timer.setIntervalTicks(oneMillisecondTicks(timer));
  timer.setCallback(timerCallback, reinterpret_cast<void*>(0x54494D45ULL));

  gActiveTimer = &timer;
  gCallbackCount = 0;
  gIrqCount = 0;
  gLastIrqId = 0;
  gCallbackContextMatched = false;

  Gic::init();
  configureTimerPpiGroup0();
  enableDistributorGroups();
  Gic::setPriorityLevel(Timer::IRQ, 0x80);
  Gic::enableIrq(Timer::IRQ);

  uint64_t savedVbar = installTestVbar();
  uint64_t savedDaif = saveDaif();
  uint64_t savedHcr = routePhysicalInterruptsToEl2();

  timer.start();
  unmaskIrqAndFiq();
  bool seen = waitForTimerCallback();
  restoreDaif(savedDaif);
  restoreHcr(savedHcr);
  timer.stop();
  restoreVbar(savedVbar);

  Gic::disableIrq(Timer::IRQ);
  gActiveTimer = nullptr;

  bool passed = seen
             && gCallbackCount != 0
             && gIrqCount != 0
             && gLastIrqId == Timer::IRQ
             && gCallbackContextMatched;
  return passed;
}

static const TestCase kTimerCases[] = {
    {"frequency_and_counter_progress",      test_frequency_and_counter_progress},
    {"elapsed_ticks_reset_on_start",        test_elapsed_ticks_reset_on_start},
    {"handle_irq_invokes_callback",         test_handle_irq_reloads_and_invokes_callback},
    {"physical_timer_irq_invokes_callback", test_physical_timer_irq_invokes_callback},
};

static const TestSuite kTimerSuite = {
    "TimerHarness",
    kTimerCases,
    4,
};

REGISTER_SUITE(kTimerSuite);
