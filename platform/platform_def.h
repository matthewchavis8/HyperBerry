/**
 * @file platform_def.h
 * @brief Selects the active platform constant set at compile time.
 * @ingroup platform
 *
 * The build chooses exactly one target platform via PLATFORM_QEMU or
 * PLATFORM_RPI5. This wrapper exposes a stable include path for code that
 * needs peripheral base addresses without caring which board is active.
 */
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
