#ifndef __TIMER_H__
#define __TIMER_H__

#include <stdint.h>

/**
 * @brief ARM generic timer driver using the EL2 physical timer (CNTHP).
 * @ingroup drivers
 *
 * Call init() once at boot, setCallback() before start(). The IRQ
 * handler must forward IRQ (26) firings to handleIrq().
 */
class Timer {
  private:
    uint64_t m_frequency{};
    uint64_t m_intervalTicks{};
    uint64_t m_lastArmTicks{};
    void (*m_callback)(void*) { nullptr };
    void* m_ctx           { nullptr };

  public:
    /** GIC PPI line wired to the ARM generic hypervisor physical timer. */
    static constexpr uint32_t IRQ = 26U;

    Timer() = default;
    Timer(const Timer&) = delete;
    Timer& operator=(const Timer&) = delete;
    Timer(Timer&&) = delete;
    Timer& operator=(Timer&&) = delete;

    /**
     * @brief Read CNTFRQ_EL0 and establish the default one-second interval.
     */
    void init() noexcept;

    /**
     * @brief Arm the physical timer with the configured interval.
     */
    void start() noexcept;

    /**
     * @brief Disarm the physical timer.
     */
    void stop() const noexcept;

    /**
     * @brief Reload the timer and invoke the registered callback.
     */
    void handleIrq() noexcept;

    /**
     * @brief Set the physical timer interval in counter ticks.
     * @param ticks Down-count value to write to CNTHP_TVAL_EL2.
     */
    void setIntervalTicks(uint64_t ticks) noexcept;

    /**
     * @brief Register a callback invoked on each timer expiry.
     * @param cb  Function to call; must be IRQ-safe.
     * @param ctx Opaque pointer forwarded to cb.
     */
    void setCallback(void (*cb)(void*), void* ctx) noexcept;

    /** @return Counter frequency in Hz (CNTFRQ_EL0). */
    [[nodiscard]] uint64_t getFrequency() const noexcept;

    /** @return Raw physical counter value (CNTVCT_EL0). */
    [[nodiscard]] uint64_t getRawCount() const noexcept;

    /** @return Ticks elapsed since the last timer arm. */
    [[nodiscard]] uint64_t getElapsedTicks() const noexcept;
};

#endif // !__TIMER_H__
