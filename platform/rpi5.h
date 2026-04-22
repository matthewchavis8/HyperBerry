#ifndef __PLATFORM_RPI5_H__
#define __PLATFORM_RPI5_H__

#include <stdint.h>

namespace Platform {
inline constexpr uint64_t kUartBase = 0x107D001000ULL;
inline constexpr uint64_t kPeripheralBase = 0x107D000000ULL;
inline constexpr uint64_t kPeripheralSize = 0x200000000ULL;
}

#endif  // !__PLATFORM_RPI5_H__
