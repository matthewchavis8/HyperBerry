// @file gic.cpp
// @brief Generic Interrupt Controller register address definitions.

#include "gic.h"
#include "lib/mmio/mmio.h"
#include "regs.inc"
#include <stdint.h>

namespace {
// Seeded from the values generated out of the board's host device tree so the
// driver is usable before any tree has been parsed. verifyBspAgainstDtb
// repoints them at what the firmware tree reports.
uint64_t gDistBase { BSP_GIC_DISTRIBUTOR_BASE };
uint64_t gCpuBase { BSP_GIC_CPU_BASE };
uint64_t gHvBase { BSP_GIC_HV_BASE };
uint64_t gVcpuBase { BSP_GIC_VCPU_BASE };

namespace GicReg {
    // Distributor
    // Global interrupt distribution registers. These control interrupt
    // enablement, pending/active state, priority, target CPUs, trigger
    // configuration, and software-generated interrupts.
    namespace Dist {
        // Enables and disables distributor forwarding.
        constexpr uintptr_t CTLR { 0x000 };
        // Reports interrupt controller type and supported interrupt count.
        constexpr uintptr_t TYPER { 0x004 };
        // Selects interrupt security group membership.
        constexpr uintptr_t IGROUPR { 0x080 };
        // Sets interrupt enable bits.
        constexpr uintptr_t ISENABLER { 0x100 };
        // Clears interrupt enable bits.
        constexpr uintptr_t ICENABLER { 0x180 };
        // Holds interrupt priority fields.
        constexpr uintptr_t IPRIORITYR { 0x400 };
        // Routes shared peripheral interrupts to target CPU interfaces.
        constexpr uintptr_t ITARGETSR { 0x800 };
        // Configures level-sensitive or edge-triggered interrupt behavior.
        constexpr uintptr_t ICFGR { 0xC00 };
        // Generates software interrupts.
        constexpr uintptr_t SGIR { 0xF00 };
        // Sets interrupt pending bits.
        constexpr uintptr_t ISPENDR { 0x200 };
        // Clears interrupt pending bits.
        constexpr uintptr_t ICPENDR { 0x280 };
        // Reports or sets interrupt active state bits.
        constexpr uintptr_t ISACTIVER { 0x300 };
        // Clears interrupt active state bits.
        constexpr uintptr_t ICACTIVER { 0x380 };
        // Group enable bit for Group 1 (Non-secure).
        constexpr uint32_t CTLR_GRPEN1 { (1U << 0) };
    } // namespace Dist

    // CPU
    // Physical CPU interface registers. These expose interrupt
    // acknowledgement, priority masking, preemption grouping, and
    // end-of-interrupt signaling to the running CPU.
    namespace Cpu {
        // Enables and disables CPU interface signaling.
        constexpr uintptr_t CTLR { 0x000 };
        // Masks interrupts below the configured priority threshold.
        constexpr uintptr_t PMR { 0x004 };
        // Acknowledges the highest-priority pending interrupt.
        constexpr uintptr_t IAR { 0x00C };
        // Signals completion of an acknowledged interrupt.
        constexpr uintptr_t EOIR { 0x010 };
        // Selects the binary point used for priority grouping and preemption.
        constexpr uintptr_t BPR { 0x008 };
        // Reports the currently running interrupt priority.
        constexpr uintptr_t RPR { 0x014 };
        // Reports the highest-priority pending interrupt.
        constexpr uintptr_t HPPIR { 0x018 };

        // Enable Group 1 interrupts
        constexpr uint32_t CTLR_GRPEN1 { (1U << 0) };
        // Split priority drop from deactivation
        constexpr uint32_t CTLR_EOIMODE { (1U << 9) };
    } // namespace Cpu

    // HV
    // Hypervisor control interface registers. These configure virtual
    // interrupt injection, expose maintenance interrupt status, and hold
    // the virtual list registers used to present interrupts to a guest.
    namespace Hv {
        // Controls virtual CPU interface operation.
        constexpr uintptr_t HCR { 0x000 };
        // Reports virtual interrupt controller capabilities.
        constexpr uintptr_t VTR { 0x004 };
        // Mirrors guest-visible CPU interface control state.
        constexpr uintptr_t VMCR { 0x008 };
        // Reports maintenance interrupt causes.
        constexpr uintptr_t MISR { 0x010 };
        // Holds virtual interrupt state for list register slot 0.
        constexpr uintptr_t LR0 { 0x100 };
        // Reports which list registers have EOIs pending.
        constexpr uintptr_t EISR { 0x020 };
        // Reports which list registers are empty.
        constexpr uintptr_t ELSR { 0x030 };
        // Tracks virtual active priorities.
        constexpr uintptr_t APR { 0x0F0 };
        // Holds virtual interrupt state for list register slot 1.
        constexpr uintptr_t LR1 { 0x104 };
        // Holds virtual interrupt state for list register slot 2.
        constexpr uintptr_t LR2 { 0x108 };
        // Holds virtual interrupt state for list register slot 3.
        constexpr uintptr_t LR3 { 0x10C };
        // enable virtual CPU interface
        constexpr uint32_t HCR_EN { (1U << 0) };
        // maintenance IRQ on empty list regs
        constexpr uint32_t HCR_UIE { (1U << 1) };
        // List register Pending bit
        constexpr uint32_t LR_PENDING { (1U << 28) };
        // List register Active bit
        constexpr uint32_t LR_ACTIVE { (1U << 29) };
        // List register Hardware bit
        constexpr uint32_t LR_HW { (1U << 31) };
        // List register Physical shift
        constexpr uint32_t LR_PHYS_SHIFT { 10 };
        // Virtual CPU interface enable bit
        constexpr uint32_t VMCR_EN0 { (1U << 0) };
        // Virtual CPU interface priority mask shift
        constexpr uint32_t VMCR_PMR_SHIFT { 27 };

        // Event injection registers
        constexpr uintptr_t EISR0 { 0x020 };
        constexpr uintptr_t EISR1 { 0x024 };
        constexpr uintptr_t ELSR0 { 0x030 };
        constexpr uintptr_t ELSR1 { 0x034 };
    } // namespace Hv

    // VCPU
    // Guest-visible virtual CPU interface registers, These offsets are
    // used by the virtual interface page to let a guest acknowledge,
    // mask, complete, and inspect virtual interrupts
    namespace Vcpu {
        // Enables and disables the guest-visible virtual CPU interface
        constexpr uintptr_t CTLR { 0x000 };
        // Masks virtual interrupts below the configured priority threshold
        constexpr uintptr_t PMR { 0x004 };
        // Lets the guest acknowledge the highest-priority pending virtual interrupt
        constexpr uintptr_t IAR { 0x00C };
        // Lets the guest signal completion of an acknowledged virtual interrupt
        constexpr uintptr_t EOIR { 0x010 };
        // Reports the currently running virtual interrupt priority
        constexpr uintptr_t RPR { 0x014 };
        // Reports the highest-priority pending virtual interrupt
        constexpr uintptr_t HPPIR { 0x018 };
    } // namespace Vcpu
} // namespace GicReg
} // namespace

void Gic::cpuInit() {
    // Figure out how many List Registers are available for virtual interrupts.
    uint32_t vtr { mmio::Read<uint32_t>(frameBase(Frame::HV), GicReg::Hv::VTR) };
    m_numLr = (vtr & 0x3F) + 1;

    // Enable Group 1 interrupts and split priority drop from deactivation
    mmio::Write<uint32_t>(frameBase(Frame::CPU),
            GicReg::Cpu::CTLR,
            GicReg::Cpu::CTLR_GRPEN1 | GicReg::Cpu::CTLR_EOIMODE);

    // Unmask all interrupts priorities
    mmio::Write<uint32_t>(frameBase(Frame::CPU), GicReg::Cpu::PMR, 0xFF);

    // Set binary point to 0 (no priority grouping)
    mmio::Write<uint32_t>(frameBase(Frame::CPU), GicReg::Cpu::BPR, 0x0);

    // Set up VMCR
    uint32_t vmcr { GicReg::Hv::VMCR_EN0 };
    vmcr |= (0xFF >> 3) << GicReg::Hv::VMCR_PMR_SHIFT;
    mmio::Write<uint32_t>(frameBase(Frame::HV), GicReg::Hv::VMCR, vmcr);

    // Bring up vGic Hypervisor interface
    mmio::Write<uint32_t>(frameBase(Frame::HV), GicReg::Hv::HCR, GicReg::Hv::HCR_EN);

    // Clear all List Register entries
    for (uint32_t i {}; i < m_numLr; i++) {
        mmio::Write<uint32_t>(frameBase(Frame::HV), GicReg::Hv::LR0 + i * 4, 0);
    }

    mmio::Write<uint32_t>(frameBase(Frame::HV), GicReg::Hv::APR, 0);
}

void Gic::CpuReset() {
    // Clear all List Register entries
    for (uint32_t i {}; i < m_numLr; i++) {
        mmio::Write<uint32_t>(frameBase(Frame::HV), GicReg::Hv::LR0 + i * 4, 0);
    }
    mmio::Write<uint32_t>(frameBase(Frame::HV), GicReg::Hv::APR, 0);
    mmio::Write<uint32_t>(frameBase(Frame::HV), GicReg::Hv::VMCR, 0);
}

void Gic::EnableIrq(uint32_t id) {
    uint32_t regIdx { id / 32 };
    uint32_t bitMsk { (1U << (id % 32)) };
    mmio::Write<uint32_t>(frameBase(Frame::DIST), GicReg::Dist::ISENABLER + regIdx * 4, bitMsk);
}

void Gic::DisableIrq(uint32_t id) {
    uint32_t regIdx { id / 32 };
    uint32_t bitMsk { (1U << (id % 32)) };
    mmio::Write<uint32_t>(frameBase(Frame::DIST), GicReg::Dist::ICENABLER + regIdx * 4, bitMsk);
}

Gic::IrqAck Gic::AckIrq() {
    uint32_t iar { mmio::Read<uint32_t>(frameBase(Frame::CPU), GicReg::Cpu::IAR) };
    return IrqAck { iar, iar & 0x3FF };
}

void Gic::EndIrq(IrqAck irq) {
    mmio::Write<uint32_t>(frameBase(Frame::CPU), GicReg::Cpu::EOIR, irq.iar);
}

int Gic::InjectIrq(uint32_t virtId, uint32_t id) {
    uint32_t elsr0 { mmio::Read<uint32_t>(frameBase(Frame::HV), GicReg::Hv::ELSR0) };
    uint32_t elsr1 { mmio::Read<uint32_t>(frameBase(Frame::HV), GicReg::Hv::ELSR1) };

    // Find a free list register slot
    int freeLr { -1 };
    for (uint32_t i {}; i < m_numLr; i++) {
        uint32_t elsr { (i < 32) ? elsr0 : elsr1 };
        uint32_t bit { i % 32 };

        // Virtual Id was already pending do not inject
        uint32_t lrVal { mmio::Read<uint32_t>(frameBase(Frame::HV), GicReg::Hv::LR0 + i * 4) };
        if ((lrVal & 0x3FF) == virtId) return -1;

        if ((elsr >> bit) & 1U) {
            if (freeLr == -1) freeLr = static_cast<int>(i);
        }
    }

    if (freeLr == -1) {
        return -1;
    }

    // Submit List Entry
    uint32_t lr { virtId & 0x3FF };
    lr |= GicReg::Hv::LR_PENDING;
    lr |= GicReg::Hv::LR_HW;
    lr |= (id & 0x3FF) << GicReg::Hv::LR_PHYS_SHIFT;
    mmio::Write<uint32_t>(
            frameBase(Frame::HV), GicReg::Hv::LR0 + static_cast<uint32_t>(freeLr) * 4, lr);

    return 0;
}

bool Gic::HasPendingIrq() {
    for (uint32_t i {}; i < m_numLr; i++) {
        uint32_t lrVal { mmio::Read<uint32_t>(frameBase(Frame::HV), GicReg::Hv::LR0 + i * 4) };
        if (lrVal & GicReg::Hv::LR_PENDING) return true;
    }
    return false;
}

void Gic::EnableMainIrq(bool isEnable) {
    // Enable/disable maintenance IRQ on empty list regs
    uint32_t hcr { mmio::Read<uint32_t>(frameBase(Frame::HV), GicReg::Hv::HCR) };
    isEnable ? hcr |= GicReg::Hv::HCR_UIE : hcr &= ~GicReg::Hv::HCR_UIE;
    mmio::Write<uint32_t>(frameBase(Frame::HV), GicReg::Hv::HCR, hcr);
}

void Gic::SetPriorityLevel(uint32_t id, uint8_t priority) {
    mmio::Write<uint8_t>(gDistBase, GicReg::Dist::IPRIORITYR + id, priority);
}

void Gic::Init() {
    // Disable Distributor
    mmio::Write<uint32_t>(frameBase(Frame::DIST), GicReg::Dist::CTLR, 0);

    // Find out how many interrupt lines are supported
    uint32_t typer { mmio::Read<uint32_t>(frameBase(Frame::DIST), GicReg::Dist::TYPER) };
    uint32_t numOfIrqLines { (typer & 0x1F) + 1 };

    // Configure all SPIs (Shared Peripheral Interrupts)
    for (uint32_t i {}; i < numOfIrqLines; i++) {
        mmio::Write<uint32_t>(frameBase(Frame::DIST),
                GicReg::Dist::IGROUPR + i * 4,
                0xFFFFFFFF); // Group all Non-secure SPIs
        mmio::Write<uint32_t>(frameBase(Frame::DIST),
                GicReg::Dist::ICENABLER + i * 4,
                0xFFFFFFFF); // Disable all SPIs
        mmio::Write<uint32_t>(frameBase(Frame::DIST),
                GicReg::Dist::ICPENDR + i * 4,
                0xFFFFFFFF); // Clear any pending SPIs
        mmio::Write<uint32_t>(frameBase(Frame::DIST),
                GicReg::Dist::ICACTIVER + i * 4,
                0xFFFFFFFF); // Clear any active SPIs
    }

    // TODO: Eventually need to come up with some sort of interrupt priority ranking
    // but for now all priorities will be set to mid
    uint32_t numOfSpis { numOfIrqLines * 32 };
    for (uint32_t i {}; i < numOfSpis; i++) {
        SetPriorityLevel(i, 0x80);
    }

    // Route all SPIs to CPU0
    for (uint32_t i { 32 }; i < numOfSpis; i += 4) {
        mmio::Write<uint32_t>(frameBase(Frame::DIST), GicReg::Dist::ITARGETSR + i, 0x01010101);
    }

    // All SPis are level triggered
    uint32_t numCfgRegs { numOfIrqLines * 2 };
    for (uint32_t i { 2 }; i < numCfgRegs; i++) {
        mmio::Write<uint32_t>(frameBase(Frame::DIST), GicReg::Dist::ICFGR + i * 4, 0x00000000);
    }

    // Re-enable Distributor
    mmio::Write<uint32_t>(frameBase(Frame::DIST), GicReg::Dist::CTLR, GicReg::Dist::CTLR_GRPEN1);

    cpuInit();
}

uintptr_t Gic::frameBase(Frame frame) {
    uint64_t base { 0 };
    switch (frame) {
        case Frame::DIST:
            base = gDistBase;
            break;
        case Frame::CPU:
            base = gCpuBase;
            break;
        case Frame::HV:
            base = gHvBase;
            break;
        case Frame::VCPU:
            base = gVcpuBase;
            break;
    }

    return static_cast<uintptr_t>(base);
}

void Gic::SetBases(uint64_t dist, uint64_t cpu, uint64_t hv, uint64_t vcpu) {
    gDistBase = dist;
    gCpuBase = cpu;
    gHvBase = hv;
    gVcpuBase = vcpu;
}

uint64_t Gic::DistBase() {
    return gDistBase;
}

uint64_t Gic::HvBase() {
    return gHvBase;
}

uint64_t Gic::VcpuBase() {
    return gVcpuBase;
}
