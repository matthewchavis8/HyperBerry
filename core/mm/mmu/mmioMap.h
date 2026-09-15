// @file mmioMap.h
// @brief Device windows handed to the MMU layers for mapping.
// @ingroup mmu
//
// The device tree is the source of truth for these addresses, but the parser
// lives in Core and Mm may not depend on Core. Mm owns the shape; whoever read
// the tree fills it in and passes it down.
#ifndef __MMIO_MAP_H__
#define __MMIO_MAP_H__

#include <stdint.h>
#include "core/mm/pageTable/pageTable.h"

// Upper bound on the windows one map can carry.
#define MMIO_MAX_WINDOWS 8

// @brief One device window to map.
// @ingroup mmu
//
// @note 16 byte aligned deliberately. A host map is built before HostMmu
//       enables stage 1, where all memory behaves as Device-nGnRnE and every
//       access must be naturally aligned. The compiler copies a struct this
//       size with 128 bit NEON pairs, which fault on an 8 byte aligned
//       address, temporaries included. @ref MemoryMap carries the same
//       attribute for the same reason.
struct alignas(16) MmioWindow {
    uint64_t base; // Address in the space being mapped: VA at EL2, IPA for a guest.
    uint64_t pa;   // Backing physical address. Equal to @p base for passthrough.
    uint64_t size; // Size in bytes, a whole multiple of the granule @p byPage selects.
    bool byPage;   // Map in 4 KiB pages rather than 2 MiB blocks.
};

// @brief A fixed capacity set of device windows.
// @ingroup mmu
//
// Adds are idempotent, so several `reg` entries that widen to the same block
// do not turn into duplicate mappings. A full map drops further windows and
// says so, because silently mapping less than the caller asked for is the
// failure mode this whole path exists to remove.
//
// @note 16 byte aligned for the reason given on @ref MmioWindow.
struct alignas(16) MmioMap {
    MmioWindow windows[MMIO_MAX_WINDOWS] {};
    uint32_t count {};

    // @brief Add an identity window over @p size bytes from @p base, widened
    //         to whole 2 MiB blocks.
    bool addBlocks(uint64_t base, uint64_t size) {
        uint64_t start = base & ~(SIZE_2MB - 1);
        uint64_t end = (base + size + SIZE_2MB - 1) & ~(SIZE_2MB - 1);
        return add(MmioWindow { start, start, end - start, false });
    }

    // @brief Add a window mapping @p ipa to @p pa in whole 4 KiB pages.
    bool addPages(uint64_t ipa, uint64_t pa, uint64_t size) {
        uint64_t start = ipa & ~(SIZE_4KB - 1);
        uint64_t end = (ipa + size + SIZE_4KB - 1) & ~(SIZE_4KB - 1);
        return add(MmioWindow { start, pa & ~(SIZE_4KB - 1), end - start, true });
    }

    // @brief True when some window already maps @p addr.
    bool covers(uint64_t addr) const {
        for (uint32_t i {}; i < count; ++i) {
            if (addr >= windows[i].base && addr < windows[i].base + windows[i].size) return true;
        }
        return false;
    }

    bool add(const MmioWindow& want) {
        for (uint32_t i {}; i < count; ++i) {
            const MmioWindow& have = windows[i];
            if (have.base == want.base && have.pa == want.pa && have.size == want.size &&
                    have.byPage == want.byPage) {
                return true;
            }
        }
        if (count == MMIO_MAX_WINDOWS) return false;
        windows[count++] = want;
        return true;
    }
};

#endif // !__MMIO_MAP_H__
