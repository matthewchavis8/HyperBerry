/**
 * @file bsp.h
 * @brief Selects the active board support package constant set at compile time.
 * @ingroup bsp
 *
 * The build chooses exactly one target BSP via BSP_QEMU or BSP_RPI5. This
 * wrapper exposes a stable include path for code that needs board constants
 * without caring which board is active.
 */
#ifndef __BSP_H__
#define __BSP_H__

#if defined(BSP_QEMU)
#include "bsp/qemu.h"
#elif defined(BSP_RPI5)
#include "bsp/rpi5.h"
#elif defined(BSP_FVP)
#include "bsp/fvp.h"
#else
#error "Unsupported BSP. Define BSP_QEMU, BSP_RPI5, or BSP_FVP."
#endif

#endif  // !__BSP_H__

