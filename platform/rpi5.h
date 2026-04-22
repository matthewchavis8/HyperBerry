/**
 * @file rpi5.h
 * @brief Platform constants for Raspberry Pi 5 hardware.
 * @ingroup platform
 */
#ifndef __PLATFORM_RPI5_H__
#define __PLATFORM_RPI5_H__

#include <stdint.h>

namespace Platform {
/** @brief PL011 UART MMIO base on BCM2712. */
inline constexpr uint64_t kUartBase = 0x107D001000ULL;
/** @brief Peripheral MMIO base mapped by the EL2 host stage-1 MMU. */
inline constexpr uint64_t kPeripheralBase = 0x107D000000ULL;
/** @brief Size of the peripheral MMIO aperture reserved for device mappings. */
inline constexpr uint64_t kPeripheralSize = 0x200000000ULL;
}

#endif  // !__PLATFORM_RPI5_H__
