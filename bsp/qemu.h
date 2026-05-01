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
inline constexpr uint64_t UART_BASE             = 0x09000000ULL;
inline constexpr uint64_t GIC_BASE              = 0x08000000ULL;
inline constexpr uint64_t GIC_DISTRIBUTOR_BASE  = 0x08000000ULL;
inline constexpr uint64_t GIC_CPU_BASE          = 0x08010000ULL;
inline constexpr uint64_t GIC_HV_BASE           = 0x08030000ULL;
inline constexpr uint64_t GIC_VCPU_BASE         = 0x08040000ULL;
inline constexpr uint64_t HV_MMIO_BASE          = 0x08000000ULL;
inline constexpr uint64_t HV_MMIO_SIZE          = 0x38000000ULL;

struct MmioRange {
  uint64_t ipa;
  uint64_t pa;
  uint64_t size;
};

inline constexpr MmioRange GUEST_MMIO[] = {
  // QEMU virt: expose GIC + PL011 to the guest for bring-up.
  {0x08000000ULL, 0x08000000ULL, 0x00200000ULL},
  {0x09000000ULL, 0x09000000ULL, 0x00200000ULL},
};

inline constexpr size_t GUEST_MMIO_COUNT = sizeof(GUEST_MMIO) / sizeof(GUEST_MMIO[0]);
}  // namespace b

#endif  // !__BSP_QEMU_H__
