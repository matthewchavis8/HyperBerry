/**
 * @file dtbMmio.h
 * @brief Derive the MMU device windows from a device tree.
 * @ingroup core
 *
 * The tree is the source of truth for peripheral addresses, so the windows the
 * two MMU layers map are read out of it rather than written down. Which
 * peripherals appear is a policy decision and lives here; where they are is
 * the tree's to say.
 */
#ifndef __DTB_MMIO_H__
#define __DTB_MMIO_H__

#include <stdint.h>
#include "core/mm/mmu/mmioMap.h"

/**
 * @brief Device windows EL2 must map into its own stage 1 tables.
 * @ingroup core
 *
 * The console and the whole interrupt controller, GICH and GICV included,
 * because EL2 drives all of it. Widened to 2 MiB blocks, which is the only
 * granule @ref HostMmu::mapRange installs.
 *
 * @param dtb Physical address of the host device tree.
 */
MmioMap dtbHostMmio(uintptr_t dtb);

/**
 * @brief Device windows a guest may reach through stage 2.
 * @ingroup core
 *
 * Page granular throughout, because the interesting question here is what the
 * guest must *not* reach and a 2 MiB block is far too coarse to answer it.
 *
 * @param guestDtb Address of the guest device tree, readable by EL2.
 */
MmioMap dtbGuestMmio(uintptr_t guestDtb);

#endif // !__DTB_MMIO_H__
