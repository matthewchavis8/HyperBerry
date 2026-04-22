#ifndef __PLATFORM_DEF_H__
#define __PLATFORM_DEF_H__

#if defined(PLATFORM_QEMU)
#include "platform/qemu.h"
#elif defined(PLATFORM_RPI5)
#include "platform/rpi5.h"
#else
#error "Unsupported platform. Define PLATFORM_QEMU or PLATFORM_RPI5."
#endif

#endif  // !__PLATFORM_DEF_H__
