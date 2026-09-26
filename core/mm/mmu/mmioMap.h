// @file mmioMap.h
// @brief Device windows handed to the MMU layers for mapping.
// @ingroup mmu
//
// The device tree is the source of truth for these addresses, but the parser
// lives in Core and Mm may not depend on Core. Mm owns the shape; whoever read
// the tree fills it in and passes it down.
#ifndef __MMIO_MAP_H__
#define __MMIO_MAP_H__

#include <array>
#include <cstdint>
#include "core/mm/pageTable/pageTable.h"

// Upper bound on the windows one map can carry.
inline constexpr uint32_t MMIO_MAX_WINDOWS { 8 };

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
class alignas(16) MmioMap {
private:
    std::array<MmioWindow, MMIO_MAX_WINDOWS> m_windows {};
    uint32_t m_count {};

public:
    // @brief Add an identity window over @p size bytes from @p base, widened
    //        to whole 2 MiB blocks.
    // @return false when the map is full.
    bool AddBlocks(uint64_t base, uint64_t size) {
        uint64_t start { base & ~(SIZE_2MB - 1) };
        uint64_t end { (base + size + SIZE_2MB - 1) & ~(SIZE_2MB - 1) };
        return Add(MmioWindow { start, start, end - start, false });
    }

    // @brief Add a window mapping @p ipa to @p pa in whole 4 KiB pages.
    // @return false when the map is full.
    bool AddPages(uint64_t ipa, uint64_t pa, uint64_t size) {
        uint64_t start { ipa & ~(SIZE_4KB - 1) };
        uint64_t end { (ipa + size + SIZE_4KB - 1) & ~(SIZE_4KB - 1) };
        return Add(MmioWindow { start, pa & ~(SIZE_4KB - 1), end - start, true });
    }

    // @brief Add @p want unless an identical window is already present.
    // @return false when the map is full.
    bool Add(const MmioWindow& want) {
        for (const MmioWindow& have : *this) {
            if (have.base == want.base && have.pa == want.pa && have.size == want.size &&
                    have.byPage == want.byPage)
                return true;
        }

        if (m_count == MMIO_MAX_WINDOWS)
            return false;

        m_windows[m_count++] = want;
        return true;
    }

    // @return true when some window already maps @p addr.
    [[nodiscard]] bool Covers(uint64_t addr) const {
        for (const MmioWindow& window : *this) {
            if (addr >= window.base && addr < window.base + window.size)
                return true;
        }

        return false;
    }

    // @return Number of windows held.
    [[nodiscard]] uint32_t GetCount() const { return m_count; }

    [[nodiscard]] const MmioWindow* begin() const { return m_windows.data(); }
    [[nodiscard]] const MmioWindow* end() const { return m_windows.data() + m_count; }
};

#endif // !__MMIO_MAP_H__
