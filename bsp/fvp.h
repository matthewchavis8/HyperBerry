/**
 * @file fvp.h
 * @brief BSP constants for the Arm FVP_Base_RevC-2xAEMvA model.
 * @ingroup bsp
 */
#ifndef __BSP_FVP_H__
#define __BSP_FVP_H__

#include "lib/array/array.h"
#include <stddef.h>
#include <stdint.h>

namespace b {
inline constexpr uint64_t HOST_DTB_BASE         = 0x88000000ULL;
inline constexpr uint64_t UART_BASE             = 0x1C090000ULL;
inline constexpr uint64_t GIC_BASE              = 0x2F000000ULL;
inline constexpr uint64_t GIC_DISTRIBUTOR_BASE  = 0x2F000000ULL;
inline constexpr uint64_t GIC_CPU_BASE          = 0x2C000000ULL;
inline constexpr uint64_t GIC_HV_BASE           = 0x2C010000ULL;
inline constexpr uint64_t GIC_VCPU_BASE         = 0x2C02F000ULL;
inline constexpr uint64_t HV_MMIO_BASE          = 0x2F000000ULL;
inline constexpr uint64_t HV_MMIO_SIZE          = 0x00200000ULL;

inline constexpr uint64_t MMIO_REGION_SIZE      = 0x00200000ULL;
inline constexpr uint64_t MMIO_PAGE_SIZE        = 0x00001000ULL;

struct MmioRange {
  uint64_t ipa;
  uint64_t pa;
  uint64_t size;
};

inline constexpr hv::array<MmioRange, 3> GUEST_MMIO = {{
  {0x2F000000ULL, 0x2F000000ULL, MMIO_REGION_SIZE},
  {0x2F200000ULL, 0x2F200000ULL, MMIO_REGION_SIZE},
  {0x1C000000ULL, 0x1C000000ULL, MMIO_REGION_SIZE},
}};

inline constexpr hv::array<MmioRange, 1> GUEST_MMIO_PAGES = {{
  {0x107D001000ULL, UART_BASE, MMIO_PAGE_SIZE},
}};

inline constexpr size_t GUEST_MMIO_COUNT       = GUEST_MMIO.size();
inline constexpr size_t GUEST_MMIO_PAGE_COUNT  = GUEST_MMIO_PAGES.size();
}  // namespace b

#endif  // !__BSP_FVP_H__
