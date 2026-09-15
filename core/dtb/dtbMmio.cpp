// @file dtbMmio.cpp
// @brief Derive the MMU device windows from a device tree.
// @ingroup core

#include "dtbMmio.h"
#include "dtb.h"
#include "regs.inc"
#include "lib/log/log.h"

namespace {

void addOrWarn(bool added, const char* what) {
    if (!added) Log::Println("[DTB][WARN] no room for the {} window; it is unmapped", what);
}

} // namespace

MmioMap DtbHostMmio(uintptr_t dtb) {
    MmioMap map {};

    DeviceNode uart { DtbFindUart(dtb) };
    if (!uart.found || uart.regionCount == 0) {
        Log::Println("[DTB][WARN] no PL011 in device tree; EL2 console window not mapped");
    } else {
        addOrWarn(map.AddBlocks(uart.regions[0].base, uart.regions[0].size), "EL2 console");
    }

    DeviceNode gic { DtbFindGic(dtb) };
    if (!gic.found || gic.regionCount == 0) {
        Log::Println("[DTB][WARN] no GIC in device tree; EL2 interrupt windows not mapped");
    } else {
        for (uint32_t i {}; i < gic.regionCount; ++i) {
            addOrWarn(map.AddBlocks(gic.regions[i].base, gic.regions[i].size), "EL2 GIC");
        }
    }

    return map;
}

MmioMap DtbGuestMmio(uintptr_t guestDtb) {
    MmioMap map {};

    DeviceNode gic { DtbFindGic(guestDtb) };
    if (!gic.found || gic.regionCount == 0) {
        Log::Println("[DTB][WARN] guest tree has no GIC; guest gets no interrupt controller");
    } else {
        // The first two regions only: the distributor and the CPU interface.
        // GICH and GICV belong to EL2, and a guest that can reach GICH
        // programs the list registers that inject its own interrupts. The
        // guest trees declare only the two frames a guest may see, so this cap
        // is a guard against a tree edit reintroducing them rather than a
        // correction of what they say today. A GICv3 tree lists GICD and the
        // redistributor, both guest visible, so the same two regions are the
        // right answer there.
        uint32_t visible { gic.regionCount < 2 ? gic.regionCount : 2 };
        for (uint32_t i {}; i < visible; ++i) {
            addOrWarn(map.AddPages(gic.regions[i].base, gic.regions[i].base, gic.regions[i].size),
                    "guest GIC");
        }
    }

    DeviceNode uart { DtbFindUart(guestDtb) };
    if (!uart.found || uart.regionCount == 0) {
        Log::Println("[DTB][WARN] guest tree has no PL011; guest console not mapped");
    } else {
        addOrWarn(map.AddPages(uart.regions[0].base, uart.regions[0].base, uart.regions[0].size),
                "guest console");
    }

    // HACK:
    // Some bring up packages poke the Raspberry Pi 5 PL011 IPA whatever board
    // they are running on, before the guest DTB console path takes over. Back
    // that IPA with this board's PL011 so the early guest console reaches a
    // real device. Neither tree can express this: it is a property of the
    // payload, not of the hardware. On rpi5 the guest console already sits
    // here, which is what the Covers() check notices.
    // TODO: Replace this with package/DTB-specific device routing.
    constexpr uint64_t kBringUpUartIpa { 0x107D001000ULL };
    if (!map.Covers(kBringUpUartIpa)) {
        addOrWarn(map.AddPages(kBringUpUartIpa, BSP_UART_BASE, SIZE_4KB), "bring up console");
    }

    return map;
}
