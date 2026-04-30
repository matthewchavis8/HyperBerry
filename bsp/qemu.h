/**
 * @file qemu.h
 * @brief BSP constants for the QEMU `virt` machine.
 * @ingroup bsp
 */
#ifndef __BSP_QEMU_H__
#define __BSP_QEMU_H__

#include <stddef.h>
#include <stdint.h>

namespace b {
inline constexpr uint64_t UartBase   = 0x09000000ULL;
inline constexpr uint64_t GicBase    = 0x08000000;
inline constexpr uint64_t HvMmioBase = 0x08000000ULL;
inline constexpr uint64_t HvMmioSize = 0x38000000ULL;

struct MmioRange {
  uint64_t ipa;
  uint64_t pa;
  uint64_t size;
};

inline constexpr MmioRange GuestMmio[] = {
  // QEMU virt: expose GIC + PL011 to the guest for bring-up.
  {0x08000000ULL, 0x08000000ULL, 0x00200000ULL},
  {0x09000000ULL, 0x09000000ULL, 0x00200000ULL},
};

inline constexpr size_t GuestMmioCount = sizeof(GuestMmio) / sizeof(GuestMmio[0]);
}  // namespace b

#endif  // !__BSP_QEMU_H__
