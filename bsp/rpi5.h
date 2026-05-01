/**
 * @file rpi5.h
 * @brief BSP constants for Raspberry Pi 5 hardware.
 * @ingroup bsp
 */
#ifndef __BSP_RPI5_H__
#define __BSP_RPI5_H__

#include <stddef.h>
#include <stdint.h>

namespace b {
inline constexpr uint64_t UART_BASE             = 0x107D001000ULL;
inline constexpr uint64_t GIC_BASE              = 0x107fff8000ULL;
inline constexpr uint64_t GIC_DISTRIBUTOR_BASE  = 0x107fff9000ULL;
inline constexpr uint64_t GIC_CPU_BASE          = 0x107fffa000ULL;
inline constexpr uint64_t GIC_HV_BASE           = 0x107fffc000ULL;
inline constexpr uint64_t GIC_VCPU_BASE         = 0x107fffe000ULL;
inline constexpr uint64_t HV_MMIO_BASE          = 0x107D000000ULL;
inline constexpr uint64_t HV_MMIO_SIZE          = 0x200000000ULL;

struct MmioRange {
  uint64_t ipa;
  uint64_t pa;
  uint64_t size;
};

// RPI5 guest MMIO exposure is board-specific and not wired yet.
inline constexpr const MmioRange* GUEST_MMIO       = nullptr;
inline constexpr size_t           GUEST_MMIO_COUNT  = 0;
}  // namespace b

#endif  // !__BSP_RPI5_H__
