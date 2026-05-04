/**
 * @file qemu.h
 * @brief BSP constants for the QEMU `virt` machine.
 * @ingroup bsp
 */
#ifndef __BSP_QEMU_H__
#define __BSP_QEMU_H__

#include "lib/array/array.h"
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

inline constexpr uint64_t MMIO_REGION_SIZE      = 0x00200000ULL;
inline constexpr uint64_t MMIO_PAGE_SIZE        = 0x00001000ULL;

struct MmioRange {
  uint64_t ipa;
  uint64_t pa;
  uint64_t size;
};

inline constexpr hv::array<MmioRange, 2> GUEST_MMIO = {{
  {GIC_BASE, GIC_BASE, MMIO_REGION_SIZE}, // GIC MMIO
  {UART_BASE, UART_BASE, MMIO_REGION_SIZE}, // UART MMIO
}};

inline constexpr hv::array<MmioRange, 1> GUEST_MMIO_PAGES = {{
  // HACK:
  // Some QEMU bring-up packages touch the Raspberry Pi 5 PL011 IPA before the
  // guest DTB console path takes over. Back that IPA with QEMU virt's PL011 so
  // early guest UART polling reaches a real device.
  // TODO: Replace this with package/DTB-specific device routing.
  {0x107D001000ULL, UART_BASE, MMIO_PAGE_SIZE},
}};

inline constexpr size_t GUEST_MMIO_COUNT = GUEST_MMIO.size();
inline constexpr size_t GUEST_MMIO_PAGE_COUNT = GUEST_MMIO_PAGES.size();
}  // namespace b

#endif  // !__BSP_QEMU_H__
