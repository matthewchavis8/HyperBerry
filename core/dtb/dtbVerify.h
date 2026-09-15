// @file core/dtb/dtbVerify.h
// @brief Cross-check the compiled BSP constants against the firmware DTB.
// @ingroup core

#ifndef __DTB_VERIFY_H__
#define __DTB_VERIFY_H__

#include <stdint.h>

// @brief Verify that the running hardware matches what the image was built for.
// @ingroup core
//
// The BSP constants are generated from a board's device tree at build time, so
// a mismatch here means the firmware moved a peripheral out from under an image
// that still has the old address compiled in. Reports every discrepancy, then
// panics, rather than letting the first MMIO access fault somewhere unrelated.
//
// @param dtb Physical address of the firmware-supplied DTB.
void verifyBspAgainstDtb(uintptr_t dtb);

#endif // __DTB_VERIFY_H__
