/**
 * @file rpi5.h
 * @brief BSP constants for Raspberry Pi 5 hardware.
 * @ingroup bsp
 */
#ifndef __BSP_RPI5_H__
#define __BSP_RPI5_H__

#include <stddef.h>
#include <stdint.h>
#include "lib/array/array.h"

namespace b {
inline constexpr uint64_t UART_BASE             = 0x107D001000ULL;

inline constexpr uint64_t GIC_BASE              = 0x107fff8000ULL;
inline constexpr uint64_t GIC_DISTRIBUTOR_BASE  = 0x107fff9000ULL;
inline constexpr uint64_t GIC_CPU_BASE          = 0x107fffa000ULL;
inline constexpr uint64_t GIC_HV_BASE           = 0x107fffc000ULL;
inline constexpr uint64_t GIC_VCPU_BASE         = 0x107fffe000ULL;

inline constexpr uint64_t TIMER_BASE            = 0x107c003000ULL;

inline constexpr uint64_t HV_MMIO_BASE          = 0x107D000000ULL;
inline constexpr uint64_t HV_MMIO_SIZE          = 0x200000000ULL;

inline constexpr uint64_t MMIO_REGION_SIZE      = 0x00200000ULL;
inline constexpr uint64_t MMIO_PAGE_SIZE        = 0x00001000ULL;

struct MmioRange {
  uint64_t ipa;
  uint64_t pa;
  uint64_t size;
};

inline constexpr hv::array<MmioRange, 1> GUEST_MMIO = {{
  // Temporary single-guest UART passthrough for Linux bring-up.
  //
  // TODO: Replace this with a virtual UART before multi-guest support. EL2
  // should own the physical UART, trap guest UART accesses, and serialize
  // per-VM console output so guests cannot reconfigure or race the shared
  // physical device.
  // IPA addr for guest
  {UART_BASE, UART_BASE, MMIO_REGION_SIZE},
}};

inline constexpr hv::array<MmioRange, 0> GUEST_MMIO_PAGES = {{}};

inline constexpr size_t GUEST_MMIO_COUNT = GUEST_MMIO.size();
inline constexpr size_t GUEST_MMIO_PAGE_COUNT = GUEST_MMIO_PAGES.size();
}  // namespace b

#endif  // !__BSP_RPI5_H__
