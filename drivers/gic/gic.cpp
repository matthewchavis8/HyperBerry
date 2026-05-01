/**
 * @file gic.cpp
 * @brief Generic Interrupt Controller register address definitions.
 */

#include "gic.h"
#include "bsp/bsp.h"
#include <stdint.h>

namespace {
    namespace GicReg {
        // Distributor
        // Global interrupt distribution registers. These control interrupt
        // enablement, pending/active state, priority, target CPUs, trigger
        // configuration, and software-generated interrupts.
        namespace Dist {
            // Base address of the distributor register frame.
            constexpr uintptr_t BASE = b::GicBase + 0x00000;

            // Enables and disables distributor forwarding.
            constexpr uintptr_t CTLR       = BASE + 0x000;
            // Reports interrupt controller type and supported interrupt count.
            constexpr uintptr_t TYPER      = BASE + 0x004;
            // Selects interrupt security group membership.
            constexpr uintptr_t IGROUPR    = BASE + 0x080;
            // Sets interrupt enable bits.
            constexpr uintptr_t ISENABLER  = BASE + 0x100;
            // Clears interrupt enable bits.
            constexpr uintptr_t ICENABLER  = BASE + 0x180;
            // Holds interrupt priority fields.
            constexpr uintptr_t IPRIORITYR = BASE + 0x400;
            // Routes shared peripheral interrupts to target CPU interfaces.
            constexpr uintptr_t ITARGETSR  = BASE + 0x800;
            // Configures level-sensitive or edge-triggered interrupt behavior.
            constexpr uintptr_t ICFGR      = BASE + 0xC00;
            // Generates software interrupts.
            constexpr uintptr_t SGIR       = BASE + 0xF00;
            // Sets interrupt pending bits.
            constexpr uintptr_t ISPENDR    = BASE + 0x200;
            // Clears interrupt pending bits.
            constexpr uintptr_t ICPENDR    = BASE + 0x280;
            // Reports or sets interrupt active state bits.
            constexpr uintptr_t ISACTIVER  = BASE + 0x300;
            // Clears interrupt active state bits.
            constexpr uintptr_t ICACTIVER  = BASE + 0x380;
            // Group enable bit for Group 1 (Non-secure).
            constexpr uint32_t CTLR_GRPEN1 = (1U << 0);
        }

        // CPU
        // Physical CPU interface registers. These expose interrupt
        // acknowledgement, priority masking, preemption grouping, and
        // end-of-interrupt signaling to the running CPU.
        namespace Cpu {
            // Base address of the physical CPU interface register frame.
            constexpr uintptr_t BASE = b::GicBase + 0x10000;

            // Enables and disables CPU interface signaling.
            constexpr uintptr_t CTLR  = BASE + 0x000;
            // Masks interrupts below the configured priority threshold.
            constexpr uintptr_t PMR   = BASE + 0x004;
            // Acknowledges the highest-priority pending interrupt.
            constexpr uintptr_t IAR   = BASE + 0x00C;
            // Signals completion of an acknowledged interrupt.
            constexpr uintptr_t EOIR  = BASE + 0x010;
            // Selects the binary point used for priority grouping and preemption.
            constexpr uintptr_t BPR   = BASE + 0x008;
            // Reports the currently running interrupt priority.
            constexpr uintptr_t RPR   = BASE + 0x014;
            // Reports the highest-priority pending interrupt.
            constexpr uintptr_t HPPIR = BASE + 0x018;

            // Enable Group 1 interrupts
            constexpr uint32_t CTLR_GRPEN1  = (1U << 0);
            // Split priority drop from deactivation
            constexpr uint32_t CTLR_EOIMODE = (1U << 9);
        }

        // HV
        // Hypervisor control interface registers. These configure virtual
        // interrupt injection, expose maintenance interrupt status, and hold
        // the virtual list registers used to present interrupts to a guest.
        namespace Hv {
            // Base address of the hypervisor interface register frame.
            constexpr uintptr_t BASE = b::GicBase + 0x30000;

            // Controls virtual CPU interface operation.
            constexpr uintptr_t HCR   = BASE + 0x000;
            // Reports virtual interrupt controller capabilities.
            constexpr uintptr_t VTR   = BASE + 0x004;
            // Mirrors guest-visible CPU interface control state.
            constexpr uintptr_t VMCR  = BASE + 0x008;
            // Reports maintenance interrupt causes.
            constexpr uintptr_t MISR  = BASE + 0x010;
            // Holds virtual interrupt state for list register slot 0.
            constexpr uintptr_t LR0   = BASE + 0x100;
            // Reports which list registers have EOIs pending.
            constexpr uintptr_t EISR  = BASE + 0x020;
            // Reports which list registers are empty.
            constexpr uintptr_t ELSR  = BASE + 0x030;
            // Tracks virtual active priorities.
            constexpr uintptr_t APR   = BASE + 0x0F0;
            // Holds virtual interrupt state for list register slot 1.
            constexpr uintptr_t LR1   = BASE + 0x104;
            // Holds virtual interrupt state for list register slot 2.
            constexpr uintptr_t LR2   = BASE + 0x108;
            // Holds virtual interrupt state for list register slot 3.
            constexpr uintptr_t LR3   = BASE + 0x10C;
            // enable virtual CPU interface
            constexpr uint32_t HCR_EN  = (1U << 0);
            // maintenance IRQ on empty list regs
            constexpr uint32_t HCR_UIE = (1U << 1);
            // List register Pending bit
            constexpr uint32_t LR_PENDING   = (1U << 28);
            // List register Active bit
            constexpr uint32_t LR_ACTIVE    = (1U << 29);
            // List register Hardware bit
            constexpr uint32_t LR_HW        = (1U << 31);
            // List register Physical shift
            constexpr uint32_t LR_PHYS_SHIFT = 10;
            // Virtual CPU interface enable bit
            constexpr uint32_t VMCR_EN0     = (1U << 0);
            // Virtual CPU interface priority mask shift
            constexpr uint32_t VMCR_PMR_SHIFT = 27;

            // Event injection registers
            constexpr uintptr_t EISR0 = BASE + 0x020;
            constexpr uintptr_t EISR1 = BASE + 0x024;
            constexpr uintptr_t ELSR0 = BASE + 0x030;
            constexpr uintptr_t ELSR1 = BASE + 0x034;
        }

        // VCPU
        // Guest-visible virtual CPU interface registers, These offsets are
        // used by the virtual interface page to let a guest acknowledge,
        // mask, complete, and inspect virtual interrupts
        namespace Vcpu {
            // Base address of the virtual CPU interface register frame
            constexpr uint64_t BASE  = b::GicBase + 0x40000;

            // Enables and disables the guest-visible virtual CPU interface
            constexpr uintptr_t CTLR  = 0x000;
            // Masks virtual interrupts below the configured priority threshold
            constexpr uintptr_t PMR   = 0x004;
            // Lets the guest acknowledge the highest-priority pending virtual interrupt
            constexpr uintptr_t IAR   = 0x00C;
            // Lets the guest signal completion of an acknowledged virtual interrupt
            constexpr uintptr_t EOIR  = 0x010;
            // Reports the currently running virtual interrupt priority
            constexpr uintptr_t RPR   = 0x014;
            // Reports the highest-priority pending virtual interrupt
            constexpr uintptr_t HPPIR = 0x018;
        }
    }
}

void Gic::cpuInit() {
    // Figure out how many List Registers are available for virtual interrupts.
    uint32_t vtr = *reg(GicReg::Hv::VTR);
    m_numLr = (vtr & 0x3F) + 1;

    // Enable Group 1 interrupts and split priority drop from deactivation
    *reg(GicReg::Cpu::CTLR) = GicReg::Cpu::CTLR_GRPEN1 | GicReg::Cpu::CTLR_EOIMODE;

    // Unmask all interrupts priorities
    *reg(GicReg::Cpu::PMR) = 0xFF;

    // Set binary point to 0 (no priority grouping)
    *reg(GicReg::Cpu::BPR) = 0x0;

    // Set up VMCR
    uint32_t vmcr = GicReg::Hv::VMCR_EN0;
    vmcr |= (0xFF >> 3) << GicReg::Hv::VMCR_PMR_SHIFT;
    *reg(GicReg::Hv::VMCR) = vmcr;

    // Bring up vGic Hypervisor interface
    *reg(GicReg::Hv::HCR) = GicReg::Hv::HCR_EN;

    // Clear all List Register entries
    for (uint32_t i{}; i < m_numLr; i++) {
        *reg(GicReg::Hv::LR0 + i * 4) = 0;
    }

    *reg(GicReg::Hv::APR) = 0;
}

void Gic::cpuReset() {
    // Clear all List Register entries
    for (uint32_t i{}; i < m_numLr; i++) {
        *reg(GicReg::Hv::LR0 + i * 4) = 0;
    }
    *reg(GicReg::Hv::APR) = 0;
    *reg(GicReg::Hv::VMCR) = 0;
}

void Gic::enableIrq(uint32_t id) {
    uint32_t regIdx = id / 32;
    uint32_t bitMsk = (1U << (id % 32));
    *reg(GicReg::Dist::ISENABLER + regIdx * 4) = bitMsk;
}

void Gic::disableIrq(uint32_t id) {
    uint32_t regIdx = id / 32;
    uint32_t bitMsk = (1U << (id % 32));
    *reg(GicReg::Dist::ICENABLER + regIdx * 4) = bitMsk;
}

Gic::IrqAck Gic::ackIrq() {
    uint32_t iar = *reg(GicReg::Cpu::IAR);
    return IrqAck{iar, iar & 0x3FF};
}

void Gic::endIrq(IrqAck irq) {
    *reg(GicReg::Cpu::EOIR) = irq.iar;
}

int Gic::injectIrq(uint32_t virtId, uint32_t id) {
    uint32_t elsr0 = *reg(GicReg::Hv::ELSR0);
    uint32_t elsr1 = *reg(GicReg::Hv::ELSR1);

    // Find a free list register slot
    int freeLr = -1;
    for (uint32_t i{}; i < m_numLr; i++) {
        uint32_t elsr = (i < 32) ? elsr0 : elsr1;
        uint32_t bit = i % 32;

        // Virtual Id was already pending do not inject
        uint32_t lrVal = *reg(GicReg::Hv::LR0 + i * 4);
        if ((lrVal & 0x3FF) == virtId)
            return -1;

        if ((elsr >> bit) & 1U) {
            if (freeLr == -1)
                freeLr = static_cast<int>(i);
        }
    }

    if (freeLr == -1) {
        return -1;
    }

    // Submit List Entry
    uint32_t lr = virtId & 0x3FF;
    lr |= GicReg::Hv::LR_PENDING;
    lr |= GicReg::Hv::LR_HW;
    lr |= (id & 0x3FF) << GicReg::Hv::LR_PHYS_SHIFT;
    *reg(GicReg::Hv::LR0 + static_cast<uint32_t>(freeLr) * 4) = lr;

    return 0;
}

bool Gic::hasPendingIrq() {
    for (uint32_t i{}; i < m_numLr; i++) {
        uint32_t lrVal = *reg(GicReg::Hv::LR0 + i * 4);
        if (lrVal & GicReg::Hv::LR_PENDING)
            return true;
    }
    return false;
}

void Gic::enableMainIrq(bool isEnable) {
    // Enable/disable maintenance IRQ on empty list regs
    uint32_t hcr = *reg(GicReg::Hv::HCR);
    isEnable ? 
        hcr |= GicReg::Hv::HCR_UIE : hcr &= ~GicReg::Hv::HCR_UIE;
    *reg(GicReg::Hv::HCR) = hcr;
}

void Gic::setPriorityLevel(uint32_t id, uint8_t priority) {
    volatile uint8_t* priorityReg = reinterpret_cast<volatile uint8_t*>(GicReg::Dist::IPRIORITYR + id);
    *priorityReg = priority;
}

void Gic::init() {
    // Disable Distributor
    *reg(GicReg::Dist::CTLR) = 0;

    // Find out how many interrupt lines are supported
    uint32_t typer = *reg(GicReg::Dist::TYPER);
    uint32_t numOfIrqLines = (typer & 0x1F) + 1;

    // Configure all SPIs (Shared Peripheral Interrupts)
    for (uint32_t i{}; i < numOfIrqLines; i++) {
        *reg(GicReg::Dist::IGROUPR + i * 4)   = 0xFFFFFFFF; // Group all Non-secure SPIs
        *reg(GicReg::Dist::ICENABLER + i * 4) = 0xFFFFFFFF; // Disable all SPIs
        *reg(GicReg::Dist::ICPENDR + i * 4)   = 0xFFFFFFFF; // Clear any pending SPIs
        *reg(GicReg::Dist::ICACTIVER + i * 4) = 0xFFFFFFFF; // Clear any active SPIs
    }

    // TODO: Eventually need to come up with some sort of interrupt priority ranking
    // but for now all priorities will be set to mid
    uint32_t numOfSpis = numOfIrqLines * 32;
    for (uint32_t i{}; i < numOfSpis; i++) {
        setPriorityLevel(i, 0x80);
    }

    // Route all SPIs to CPU0
    for (uint32_t i{32}; i < numOfSpis; i += 4) {
        *reg(GicReg::Dist::ITARGETSR + i * 4) = 0x01010101;
    }

    // All SPis are level triggered
    uint32_t numCfgRegs = numOfIrqLines * 2;
    for (uint32_t i {2}; i < numCfgRegs; i++) {
        *reg(GicReg::Dist::ICFGR + i * 4) = 0x00000000;
    }

    // Re-enable Distributor
    *reg(GicReg::Dist::CTLR) = GicReg::Dist::CTLR_GRPEN1;

    cpuInit();
}

volatile uint32_t* Gic::reg(const uintptr_t REG) {
    return reinterpret_cast<volatile uint32_t*>(REG);
}
