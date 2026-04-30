/**
 * @file gic.h
 * @brief ARM GICv2 Distributor and CPU Interface driver.
 * @ingroup drivers_gic
 *
 * Call @ref Gic::init() once during platform bring-up before enabling IRQs.
 */

#ifndef __GIC_H__
#define __GIC_H__

#include <stdint.h>

/**
 * @brief GICv2 Distributor and CPU Interface access helpers.
 * @ingroup drivers_gic
 */
class Gic {
    private:
        /**
         * @brief Convert a physical GIC register address into a volatile 32-bit pointer.
         *
         * @param REG Absolute memory-mapped register address.
         * @return Pointer suitable for 32-bit MMIO access.
         */
        static volatile uint32_t* reg(const uintptr_t REG);

        /**
         * @brief Initialize the GICC for the current CPU.
         * @note Called internally by @ref init().
         */
        static void cpuInit();

        /** @brief Number of List Registers reported by GICH_VTR. */
        inline static uint32_t m_numLr = 0;

    public:
        struct IrqAck {
            uint32_t iar;
            uint32_t id;
        };

        /**
         * @brief Initialize the GICD and GICC.
         * @note Must be called once before any interrupt management.
         */
        static void init();

        /**
         * @brief Enable a physical interrupt at the Distributor.
         * @param id GIC interrupt ID (SPI, PPI, or SGI).
         */
        static void enableIrq(uint32_t id);

        /**
         * @brief Disable a physical interrupt at the Distributor.
         * @param id GIC interrupt ID.
         */
        static void disableIrq(uint32_t id);

        /**
         * @brief Acknowledge the highest-priority pending interrupt via GICC_IAR.
         *
         * @return Acknowledgement token plus decoded interrupt ID.
         */
        [[nodiscard]] static IrqAck ackIrq();

        /**
         * @brief Signal end-of-interrupt by writing GICC_EOIR.
         * @param irq Acknowledgement token returned by @ref ackIrq().
         */
        static void endIrq(IrqAck irq);

        /**
         * @brief Inject a virtual interrupt into a guest via a GICH List Register.
         *
         * @param virtId Virtual interrupt ID to present to the guest.
         * @param id     Physical interrupt ID backing the virtual interrupt.
         * @return 0 on success, -1 if no free List Register is available.
         */
        [[nodiscard]] static int injectIrq(uint32_t virtId, uint32_t id);

        /**
         * @brief Check whether any virtual interrupt is pending in the GICH.
         * @return @c true if at least one virtual interrupt is pending.
         */
        [[nodiscard]] static bool hasPendingIrq();

        /**
         * @brief Enable or disable the CPU Interface via GICC_CTLR.
         * @param isEnable @c true to enable, @c false to disable.
         */
        static void enableMainIrq(bool isEnable);

        /**
         * @brief Set the priority level for a physical interrupt.
         *
         * @param id       GIC interrupt ID to configure.
         * @param priority Priority value (0 = highest, 255 = lowest).
         */
        static void setPriorityLevel(uint32_t id, uint8_t priority);

        /**
         * @brief Reset the GICC to its power-on state for this CPU.
         */
        static void cpuReset();
};

#endif // __GIC_H__
