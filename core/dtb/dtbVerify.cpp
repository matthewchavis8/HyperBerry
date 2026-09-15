// @file core/dtb/dtbVerify.cpp
// @brief Cross-check the generated BSP constants against the firmware DTB.
// @ingroup core
//
// Nothing in the build regenerates a board's checked in host DTB when its
// source tree changes, so this panic is the only thing standing between a
// stale blob and a hypervisor driving the wrong addresses.

#include "dtbVerify.h"
#include "dtb.h"
#include "regs.inc"
#include "lib/log/log.h"
#include "uart.h"
#include "gic.h"
#include "panic.h"

namespace {

bool check(const char* what, uint64_t expected, uint64_t actual) {
    if (expected == actual) return true;

    Log::Println(
            "[DTB][MISMATCH] {}: built for {:x}, device tree says {:x}", what, expected, actual);
    return false;
}

} // namespace

void VerifyBspAgainstDtb(uintptr_t dtb) {
    bool ok = true;

    DeviceNode uart = DtbFindUart(dtb);
    if (!uart.found || uart.regionCount == 0) {
        Log::Println("[DTB][WARN] no PL011 in device tree; cannot verify UART_BASE");
    } else {
        // Point the driver at what the tree describes. On a matching board
        // this is the value it already had. It does not rescue a wrong
        // compile-time base: the early console faults long before this runs,
        // so a UART mismatch stays undiagnosable without a second channel.
        Uart::GetInstance().SetBase(uart.regions[0].base);
        ok &= check("UART_BASE", BSP_UART_BASE, uart.regions[0].base);
        ok &= check("UART_SIZE", BSP_UART_SIZE, uart.regions[0].size);
    }

    DeviceNode gic = DtbFindGic(dtb);
    if (!gic.found || gic.regionCount == 0) {
        Log::Println("[DTB][WARN] no GIC in device tree; cannot verify GIC bases");
    } else {
        ok &= check("GIC_DISTRIBUTOR_BASE", BSP_GIC_DISTRIBUTOR_BASE, gic.regions[0].base);
        ok &= check("GIC_DISTRIBUTOR_SIZE", BSP_GIC_DISTRIBUTOR_SIZE, gic.regions[0].size);

        // A GICv2 tree lists GICD/GICC/GICH/GICV. A GICv3 tree lists GICD and a
        // redistributor instead, so only the distributor is comparable there.
        if (gic.regionCount >= 4) {
            ok &= check("GIC_CPU_BASE", BSP_GIC_CPU_BASE, gic.regions[1].base);
            ok &= check("GIC_CPU_SIZE", BSP_GIC_CPU_SIZE, gic.regions[1].size);
            ok &= check("GIC_HV_BASE", BSP_GIC_HV_BASE, gic.regions[2].base);
            ok &= check("GIC_HV_SIZE", BSP_GIC_HV_SIZE, gic.regions[2].size);
            ok &= check("GIC_VCPU_BASE", BSP_GIC_VCPU_BASE, gic.regions[3].base);
            ok &= check("GIC_VCPU_SIZE", BSP_GIC_VCPU_SIZE, gic.regions[3].size);
            // Point the driver at the frames the tree describes. The compiled
            // values are checked rather than trusted, so on a matching board
            // this changes nothing; it means the driver follows the tree if the
            // panic below is ever relaxed.
            Gic::SetBases(gic.regions[0].base,
                    gic.regions[1].base,
                    gic.regions[2].base,
                    gic.regions[3].base);
        } else {
            Log::Println("[DTB][WARN] GIC exposes {} region(s); CPU/HV/VCPU not described",
                    gic.regionCount);
        }
    }

    if (!ok) {
        HvPanic("[ERROR][DTB] BSP constants do not match the firmware device tree");
    }

    Log::Println("[DTB] BSP constants match the device tree");
}
