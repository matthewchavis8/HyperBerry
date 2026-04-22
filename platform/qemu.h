#ifndef __PLATFORM_QEMU_H__
#define __PLATFORM_QEMU_H__

#include <stdint.h>

namespace Platform {
inline constexpr uint64_t kUartBase = 0x09000000ULL;
inline constexpr uint64_t kPeripheralBase = 0x08000000ULL;
inline constexpr uint64_t kPeripheralSize = 0x38000000ULL;
}

#endif  // !__PLATFORM_QEMU_H__
