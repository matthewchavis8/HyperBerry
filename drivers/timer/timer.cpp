#include "timer.h"
#include <stdint.h>

/** Counter frequency register — Hz at which CNTVCT_EL0 increments. */
static inline uint64_t readCntFrqEl0() {
  uint64_t reg;
  asm volatile("mrs %0, cntfrq_el0" : "=r"(reg));
  return reg;
}

/** Virtual count register — free-running tick counter. */
static inline uint64_t readCntVctEl0() {
  uint64_t reg;
  asm volatile("mrs %0, cntvct_el0" : "=r"(reg));
  return reg;
}

/** EL2 physical timer value register — fires IRQ when it counts down to zero. */
static inline void writeCnthpTvalEl2(uint64_t reg) {
  asm volatile("msr cnthp_tval_el2, %0" :: "r"(reg));
}

/** EL2 physical timer control register — bit 0: ENABLE, bit 1: IMASK, bit 2: ISTATUS. */
static inline void writeCnthpCtlEl2(uint64_t reg) {
  asm volatile(
    "msr cnthp_ctl_el2, %0\n"
    "isb"
    :: "r"(reg)
    : "memory"
  );
}

void Timer::init() noexcept {
  m_frequency = readCntFrqEl0();
  m_intervalTicks = m_frequency;
  m_lastArmTicks = readCntVctEl0();
  stop();
}

void Timer::start() noexcept {
  m_lastArmTicks = readCntVctEl0();
  writeCnthpTvalEl2(m_intervalTicks);
  constexpr uint64_t CNTHP_CTL_ENABLE  = 0b01; // ENABLE=1, IMASK=0
  writeCnthpCtlEl2(CNTHP_CTL_ENABLE);
}

void Timer::stop() const noexcept {
  constexpr uint64_t CNTHP_CTL_DISABLE = 0b00;
  writeCnthpCtlEl2(CNTHP_CTL_DISABLE);
}

void Timer::handleIrq() noexcept {
  m_lastArmTicks = readCntVctEl0();
  writeCnthpTvalEl2(m_intervalTicks);

  if (m_callback != nullptr)
    m_callback(m_ctx);
}

void Timer::setIntervalTicks(uint64_t ticks) noexcept {
  m_intervalTicks = ticks;
}

void Timer::setCallback(void (*cb)(void*), void* ctx) noexcept {
  m_callback = cb;
  m_ctx      = ctx;
}

uint64_t Timer::getFrequency() const noexcept { return m_frequency; }

uint64_t Timer::getRawCount() const noexcept { return readCntVctEl0(); }
uint64_t Timer::getElapsedTicks() const noexcept { return readCntVctEl0() - m_lastArmTicks; }
