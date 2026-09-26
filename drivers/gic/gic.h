// @file gic.h
// @brief ARM GICv2 Distributor and CPU Interface driver.
// @ingroup gic
//
// @ref Gic::GetInstance() brings the controller up on first call.

#ifndef __GIC_H__
#define __GIC_H__

#include <cstdint>

// @brief GICv2 Distributor and CPU Interface driver.
// @ingroup gic
class Gic {
private:
    // Which register frame an offset is measured from.
    enum class Frame { DIST, CPU, HV, VCPU };

    uint64_t m_distBase;
    uint64_t m_cpuBase;
    uint64_t m_hvBase;
    uint64_t m_vcpuBase;
    uint32_t m_numLr {}; // List Registers reported by GICH_VTR

    // @brief Bind to the frames generated from the board's host tree and
    //        bring the controller up.
    //
    // Private because @ref GetInstance() is the only way to reach the one
    // physical GIC. Defined out of line so `regs.inc` stays in the .cpp.
    Gic();

    [[nodiscard]] uintptr_t getFrameBase(Frame frame) const;

    // @brief Initialize the GICC for the current CPU.
    // @return Nothing.
    void cpuInit();

public:
    struct IrqAck {
        uint32_t iar;
        uint32_t id;
    };

    // @brief The board's one physical GIC.
    //
    // Constructed on first call rather than at static-init time.
    //
    // @return Reference to the single driver instance.
    static Gic& GetInstance();

    // @brief Point the driver at the register frames a device tree describes.
    //
    // The frames start at the values generated from the board's host tree.
    // Discovery repoints them at what the firmware tree actually reports and
    // brings the controller up there, the way @ref Uart::SetBase does for
    // the console. Unchanged bases leave the controller alone.
    //
    // @return Nothing.
    void SetBases(uint64_t dist, uint64_t cpu, uint64_t hv, uint64_t vcpu);

    // @return Base of the distributor frame in use.
    [[nodiscard]] uint64_t GetDistBase() const;

    // @return Base of the hypervisor interface frame in use.
    [[nodiscard]] uint64_t GetHvBase() const;

    // @return Base of the guest facing virtual CPU interface frame in use.
    [[nodiscard]] uint64_t GetVcpuBase() const;

    // @brief Reprogram the distributor and CPU interface into the state
    //        construction leaves them in.
    //
    // Construction already does this, so nothing has to call it before
    // using the driver. It exists to bring the controller back after
    // @ref CpuReset() or anything else that disturbed it.
    //
    // @return Nothing.
    void Reset();

    // @brief Enable a physical interrupt at the Distributor.
    // @param id GIC interrupt ID (SPI, PPI, or SGI).
    // @return Nothing.
    void EnableIrq(uint32_t id);

    // @brief Disable a physical interrupt at the Distributor.
    // @param id GIC interrupt ID.
    // @return Nothing.
    void DisableIrq(uint32_t id);

    // @brief Acknowledge the highest-priority pending interrupt via GICC_IAR.
    // @return Acknowledgement token plus decoded interrupt ID.
    [[nodiscard]] IrqAck AckIrq();

    // @brief Signal end-of-interrupt by writing GICC_EOIR.
    // @param irq Acknowledgement token returned by @ref AckIrq().
    // @return Nothing.
    void EndIrq(IrqAck irq);

    // @brief Inject a virtual interrupt into a guest via a GICH List Register.
    // @param virtId Virtual interrupt ID to present to the guest.
    // @param id     Physical interrupt ID backing the virtual interrupt.
    // @return 0 on success, -1 if no free List Register is available.
    [[nodiscard]] int InjectIrq(uint32_t virtId, uint32_t id);

    // @brief Check whether any virtual interrupt is pending in the GICH.
    // @return @c true if at least one virtual interrupt is pending.
    [[nodiscard]] bool HasPendingIrq();

    // @brief Enable or disable the maintenance interrupt via GICH_HCR.UIE.
    // @param isEnable @c true to enable, @c false to disable.
    // @return Nothing.
    void EnableMainIrq(bool isEnable);

    // @brief Set the priority level for a physical interrupt.
    // @param id       GIC interrupt ID to configure.
    // @param priority Priority value (0 = highest, 255 = lowest).
    // @return Nothing.
    void SetPriorityLevel(uint32_t id, uint8_t priority);

    // @brief Reset the GICC to its power-on state for this CPU.
    // @return Nothing.
    void CpuReset();

    Gic(const Gic&) = delete;
    Gic& operator=(const Gic&) = delete;
    Gic(Gic&&) = delete;
    Gic& operator=(Gic&&) = delete;
    ~Gic() = default;
};

#endif // __GIC_H__
