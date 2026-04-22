/**
 * @file qemu.h
 * @brief Platform constants for the QEMU `virt` machine.
 * @ingroup platform
 */
#ifndef __PLATFORM_QEMU_H__
#define __PLATFORM_QEMU_H__

#include <stdint.h>

namespace Platform {
/** @brief PL011 UART MMIO base for QEMU `virt`. */
inline constexpr uint64_t kUartBase = 0x09000000ULL;
/** @brief Start of the device-mapped MMIO aperture used by the hypervisor. */
inline constexpr uint64_t kPeripheralBase = 0x08000000ULL;
/** @brief Size of the MMIO aperture identity-mapped for peripherals. */
inline constexpr uint64_t kPeripheralSize = 0x38000000ULL;
}

#endif  // !__PLATFORM_QEMU_H__
