// @file deviceTree.h
// @brief Flattened Device Tree parsing interface.
// @ingroup core
//
// Defines the boot-time memory map extracted from the firmware-provided
// DTB and exposes the parser used during early hypervisor initialisation.

#ifndef __DEVICE_TREE_H__
#define __DEVICE_TREE_H__
#include <stdint.h>
#include <span>
#include <string_view>
#include "core/mm/mmu/mmioMap.h"

// @brief Boot-time physical memory layout parsed from the DTB.
// @ingroup core
//
// Contains the primary RAM range plus reserved regions needed by the
// hypervisor before the MMU and dynamic allocation are available.
struct alignas(16) MemoryMap {
    uint64_t memBase;         // Base physical address of the main RAM region.
    uint64_t memSize;         // Size in bytes of the main RAM region.
    uint64_t atfBase;         // Base physical address of the TF-A reserved region.
    uint64_t atfSize;         // Size in bytes of the TF-A reserved region.
    uint64_t dtbBase;         // Base physical address of the DTB blob.
    uint64_t dtbSize;         // Total size in bytes of the DTB blob.
    uint64_t bootArchiveBase; // Base PA of the firmware-loaded guest archive; zero if absent.
    uint64_t bootArchiveSize; // Size of the firmware-loaded guest archive; zero if absent.
};

// Maximum `reg` regions recorded per discovered device.
constexpr uint32_t DT_MAX_REGIONS { 8 };

// @brief One `reg` entry, translated to a CPU-physical address.
// @ingroup core
struct DeviceRegion {
    uint64_t base; // CPU-physical base address.
    uint64_t size; // Region size in bytes.
};

// @brief A device located in the DTB by compatible string.
// @ingroup core
struct alignas(16) DeviceNode {
    DeviceRegion regions[DT_MAX_REGIONS];
    uint32_t regionCount; // Number of regions decoded, capped at DT_MAX_REGIONS.
    bool found;           // True when a node matched one of the compatible strings.
};

class TreeParser {
private:
    uintptr_t m_dtb {};

    void validateHeader() const;

public:
    /// @brief Bind the parser to a readable Flattened Device Tree blob.
    /// @param dtb Physical address of the DTB.
    constexpr explicit TreeParser(uintptr_t dtb) : m_dtb { dtb } {}

    /// @brief Parse the boot memory layout from the DTB.
    /// @return The parsed memory map. Panics when the DTB is invalid or lacks memory.
    MemoryMap ParseMemoryMap() const;

    /// @brief Find the first device matching a compatible string.
    /// @param compatibles Compatible strings to match.
    /// @return The matched device, or a node with @c found clear when absent.
    DeviceNode FindCompatible(std::span<const std::string_view> compatibles) const;

    /// @brief Find the host console UART.
    /// @return The UART device, or a node with @c found clear when absent.
    DeviceNode FindUart() const;

    /// @brief Find the host interrupt controller.
    /// @return The GIC device, or a node with @c found clear when absent.
    DeviceNode FindGic() const;

    /// @brief Build and validate host MMIO mappings.
    /// @return The host MMIO map. Panics when required hardware is absent or unmappable.
    MmioMap GetHostMmio() const;

    /// @brief Build guest MMIO mappings.
    /// @return The guest MMIO map. Optional absent devices produce warnings.
    MmioMap GetGuestMmio() const;
};

#endif // __DEVICE_TREE_H__
