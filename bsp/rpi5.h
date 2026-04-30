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
inline constexpr uint64_t UartBase   = 0x107D001000ULL;
inline constexpr uint64_t GicBase    = 0x107fff8000ULL;
inline constexpr uint64_t HvMmioBase = 0x107D000000ULL;
inline constexpr uint64_t HvMmioSize = 0x200000000ULL;

struct MmioRange {
  uint64_t ipa;
  uint64_t pa;
  uint64_t size;
};

// RPI5 guest MMIO exposure is board-specific and not wired yet.
inline constexpr const MmioRange* GuestMmio = nullptr;
inline constexpr size_t GuestMmioCount = 0;
}  // namespace b

#endif  // !__BSP_RPI5_H__
