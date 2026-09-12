/**
 * @file core/dtb/dtbVerify.cpp
 * @brief Cross-check the compiled BSP constants against the firmware DTB.
 * @ingroup core
 */

#include "dtbVerify.h"
#include "dtb.h"
#include "bsp.h"
#include "uart.h"
#include "panic.h"

namespace {

const char* const kUartCompatible[] = { "arm,pl011" };

const char* const kGicCompatible[] = {
    "arm,gic-400",
    "arm,cortex-a15-gic",
    "arm,gic-v2",
    "arm,gic-v3",
};

bool check(const char* what, uint64_t expected, uint64_t actual) {
    if (expected == actual) return true;

    Uart::println("[DTB][MISMATCH] {}: built for {:x}, device tree says {:x}",
            what,
            expected,
            actual);
    return false;
}

} // namespace

void verifyBspAgainstDtb(uintptr_t dtb) {
    bool ok = true;

    DeviceNode uart = dtbFindCompatible(dtb, kUartCompatible, 1);
    if (!uart.found || uart.regionCount == 0) {
        Uart::println("[DTB][WARN] no PL011 in device tree; cannot verify UART_BASE");
    } else {
        ok &= check("UART_BASE", b::UART_BASE, uart.regions[0].base);
    }

    DeviceNode gic = dtbFindCompatible(dtb, kGicCompatible, 4);
    if (!gic.found || gic.regionCount == 0) {
        Uart::println("[DTB][WARN] no GIC in device tree; cannot verify GIC bases");
    } else {
        ok &= check("GIC_DISTRIBUTOR_BASE", b::GIC_DISTRIBUTOR_BASE, gic.regions[0].base);

        // A GICv2 tree lists GICD/GICC/GICH/GICV. A GICv3 tree lists GICD and a
        // redistributor instead, so only the distributor is comparable there.
        if (gic.regionCount >= 4) {
            ok &= check("GIC_CPU_BASE", b::GIC_CPU_BASE, gic.regions[1].base);
            ok &= check("GIC_HV_BASE", b::GIC_HV_BASE, gic.regions[2].base);
            ok &= check("GIC_VCPU_BASE", b::GIC_VCPU_BASE, gic.regions[3].base);
        } else {
            Uart::println("[DTB][WARN] GIC exposes {} region(s); CPU/HV/VCPU not described",
                    gic.regionCount);
        }
    }

    if (!ok) {
        hv_panic("[ERROR][DTB] BSP constants do not match the firmware device tree");
    }

    Uart::println("[DTB] BSP constants match the device tree");
}
