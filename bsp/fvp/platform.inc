/**
 * @file bsp/fvp/platform.inc
 * @brief FVP load-address conventions. Assembler-safe.
 * @ingroup bsp
 *
 * Not derivable from the device tree: this is where the model is told to place
 * the blob, so boot.S, bsp.h and the run target must agree on one value.
 */

#ifndef __BSP_FVP_PLATFORM_INC__
#define __BSP_FVP_PLATFORM_INC__

#define BSP_HOST_DTB_BASE      0x88000000
#define BSP_GUEST_PACKAGE_BASE 0x90000000

#endif // __BSP_FVP_PLATFORM_INC__
