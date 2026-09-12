/**
 * @file bsp/fvp/bsp.h
 * @brief BSP constants for the Arm FVP_Base_RevC-2xAEMvA model.
 * @ingroup bsp
 */
#ifndef __BSP_FVP_H__
#define __BSP_FVP_H__

#include "platform.inc"
#include "regs.inc"
#include "lib/array/array.h"
#include <stddef.h>
#include <stdint.h>

namespace b {
inline constexpr uint64_t HOST_DTB_BASE = BSP_HOST_DTB_BASE;
inline constexpr uint64_t UART_BASE = BSP_UART_BASE;

inline constexpr uint64_t GIC_BASE = 0x2F000000ULL;
inline constexpr uint64_t GIC_DISTRIBUTOR_BASE = BSP_GIC_DISTRIBUTOR_BASE;
// TODO: bsp/fvp/dts/host-fvp.dts declares arm,gic-v3, whose reg is GICD +
// GICR with no memory-mapped CPU interface, while drivers/gic is a GICv2
// driver. These three are hand-written GICv2 addresses that the FVP device
// tree does not describe, so they cannot be generated and are unverified.
inline constexpr uint64_t GIC_CPU_BASE = 0x2C000000ULL;
inline constexpr uint64_t GIC_HV_BASE = 0x2C010000ULL;
inline constexpr uint64_t GIC_VCPU_BASE = 0x2C02F000ULL;
inline constexpr uint64_t GIC_ITS_MMIO_BASE = 0x2F200000ULL;
inline constexpr uint64_t GIC_ITS_MMIO_SIZE = 0x00200000ULL;

inline constexpr uint64_t HV_MMIO_BASE = 0x2F000000ULL;
inline constexpr uint64_t HV_MMIO_SIZE = 0x00200000ULL;
inline constexpr uint64_t PLATFORM_MMIO_BASE = 0x1C000000ULL;
inline constexpr uint64_t PLATFORM_MMIO_SIZE = 0x00200000ULL;
inline constexpr uint64_t MMIO_REGION_SIZE = 0x00200000ULL;
inline constexpr uint64_t MMIO_PAGE_SIZE = 0x00001000ULL;

struct MmioRange {
    uint64_t ipa;
    uint64_t pa;
    uint64_t size;
};

inline constexpr hv::array<MmioRange, 3> GUEST_MMIO = { {
        { GIC_BASE, GIC_BASE, MMIO_REGION_SIZE }, // GIC distributor + CPU interface window
        { GIC_ITS_MMIO_BASE, GIC_ITS_MMIO_BASE, GIC_ITS_MMIO_SIZE }, // GIC ITS window
        { PLATFORM_MMIO_BASE,
                PLATFORM_MMIO_BASE,
                PLATFORM_MMIO_SIZE }, // Platform peripheral window (includes PL011 UART)
} };

// Guest kernels see the rpi5 UART baseaddress so we just catch them and route to Uart
inline constexpr hv::array<MmioRange, 1> GUEST_MMIO_PAGES = { {
        { 0x107D001000ULL, UART_BASE, MMIO_PAGE_SIZE },
} };

inline constexpr size_t GUEST_MMIO_COUNT = GUEST_MMIO.size();
inline constexpr size_t GUEST_MMIO_PAGE_COUNT = GUEST_MMIO_PAGES.size();
} // namespace b

#endif // !__BSP_FVP_H__
