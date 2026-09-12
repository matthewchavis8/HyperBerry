/**
 * @file dtbMmio.cpp
 * @brief Derive the MMU device windows from a device tree.
 * @ingroup core
 */

#include "dtbMmio.h"
#include "dtb.h"
#include "regs.inc"
#include "uart.h"

namespace {

void addOrWarn(bool added, const char* what) {
    if (!added) Uart::println("[DTB][WARN] no room for the {} window; it is unmapped", what);
}

} // namespace

MmioMap dtbHostMmio(uintptr_t dtb) {
    MmioMap map {};

    DeviceNode uart = dtbFindUart(dtb);
    if (!uart.found || uart.regionCount == 0) {
        Uart::println("[DTB][WARN] no PL011 in device tree; EL2 console window not mapped");
    } else {
        addOrWarn(map.addBlocks(uart.regions[0].base, uart.regions[0].size), "EL2 console");
    }

    DeviceNode gic = dtbFindGic(dtb);
    if (!gic.found || gic.regionCount == 0) {
        Uart::println("[DTB][WARN] no GIC in device tree; EL2 interrupt windows not mapped");
    } else {
        for (uint32_t i {}; i < gic.regionCount; ++i) {
            addOrWarn(map.addBlocks(gic.regions[i].base, gic.regions[i].size), "EL2 GIC");
        }
    }

    return map;
}

MmioMap dtbGuestMmio(uintptr_t guestDtb) {
    MmioMap map {};

    DeviceNode gic = dtbFindGic(guestDtb);
    if (!gic.found || gic.regionCount == 0) {
        Uart::println("[DTB][WARN] guest tree has no GIC; guest gets no interrupt controller");
    } else {
        // The first two regions only. A GICv2 tree lists GICD, GICC, GICH and
        // GICV, and the last two belong to EL2: a guest that can reach GICH
        // programs its own list registers and injects whatever it likes. Both
        // guest trees in this repo advertise all four, so the exclusion has to
        // be enforced here rather than trusted to the tree. A GICv3 tree lists
        // GICD and the redistributor, which are both guest visible, so the
        // same first two regions are the right answer there.
        uint32_t visible = gic.regionCount < 2 ? gic.regionCount : 2;
        for (uint32_t i {}; i < visible; ++i) {
            addOrWarn(map.addPages(gic.regions[i].base, gic.regions[i].base, gic.regions[i].size),
                    "guest GIC");
        }
    }

    DeviceNode uart = dtbFindUart(guestDtb);
    if (!uart.found || uart.regionCount == 0) {
        Uart::println("[DTB][WARN] guest tree has no PL011; guest console not mapped");
    } else {
        addOrWarn(map.addPages(uart.regions[0].base, uart.regions[0].base, uart.regions[0].size),
                "guest console");
    }

    // HACK:
    // Some bring up packages poke the Raspberry Pi 5 PL011 IPA whatever board
    // they are running on, before the guest DTB console path takes over. Back
    // that IPA with this board's PL011 so the early guest console reaches a
    // real device. Neither tree can express this: it is a property of the
    // payload, not of the hardware. On rpi5 the guest console already sits
    // here, which is what the covers() check notices.
    // TODO: Replace this with package/DTB-specific device routing.
    constexpr uint64_t kBringUpUartIpa = 0x107D001000ULL;
    if (!map.covers(kBringUpUartIpa)) {
        addOrWarn(map.addPages(kBringUpUartIpa, BSP_UART_BASE, SIZE_4KB), "bring up console");
    }

    return map;
}
