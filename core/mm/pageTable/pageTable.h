// @file pageTable.h
// @brief Shared page-table primitives for stage-1 and stage-2 MMUs.
// @ingroup mm
//
// Descriptor bits, index macros, and the generic walk / allocTable used
// by both HostMmu (EL2 stage-1) and GuestMmu (stage-2). Stage-specific
// attribute bits (MAIR index, S2AP, MemAttr) live with their owning MMU.
#ifndef __PAGE_TABLE_H__
#define __PAGE_TABLE_H__

#include <cstddef>
#include <cstdint>

// Descriptor bit positions identical in stage-1 and stage-2 VMSAv8-64.
#define PTE_VALID (1ULL << 0)
#define PTE_TABLE (1ULL << 1)
#define PTE_BLOCK (0ULL << 1)
#define PTE_AF (1ULL << 10)

// Output-address mask (bits [47:12]).
#define PTE_ADDR_MASK 0x0000FFFFFFFFF000ULL

// Granule and block sizes.
#define SIZE_4KB 0x1000ULL
#define SIZE_2MB 0x200000ULL
#define SIZE_1GB 0x40000000ULL
#define SIZE_8GB 0x200000000ULL
#define SIZE_16GB 0x400000000ULL

// Table-index extraction from an address.
#define L0_INDEX(addr) (((addr) >> 39) & 0x1FFULL)
#define L1_INDEX(addr) (((addr) >> 30) & 0x1FFULL)
#define L2_INDEX(addr) (((addr) >> 21) & 0x1FFULL)
#define L3_INDEX(addr) (((addr) >> 12) & 0x1FFULL)

static inline uint64_t* pte_next_table(uint64_t entry) {
    return (uint64_t*)(uintptr_t)(entry & PTE_ADDR_MASK);
}

static inline int pte_is_valid(uint64_t entry) {
    return (int)(entry & PTE_VALID);
}

static inline int pte_is_table(uint64_t entry) {
    return (entry & (PTE_VALID | PTE_TABLE)) == (PTE_VALID | PTE_TABLE);
}

static inline int pte_is_block(uint64_t entry) {
    return (entry & (PTE_VALID | PTE_TABLE)) == PTE_VALID;
}

// @brief A translation table rooted at one page, walked down to its L2 entries.
// @ingroup mm
//
// `startLevel == 0` corresponds to stage-1 with T0SZ=16 (48-bit VA).
// `startLevel == 1` corresponds to 40-bit stage-2 with two concatenated
// L1 root tables, which use a 10-bit root index set via @p rootIndexMask.
// Walks terminate at the L2 entry; callers map 2 MiB blocks or split them.
// The table does not own @p root; whoever allocated it frees it.
class PageTable {
private:
    uint64_t* m_root;
    uint32_t m_startLevel;
    uint64_t m_rootIndexMask; // 0x1FF, or 0x3FF for concatenated stage-2 roots

public:
    // @brief Describe the table rooted at @p root.
    // @param root          Root table (stage-1 L0 or stage-2 L1).
    // @param startLevel    Level @p root sits at.
    // @param rootIndexMask Mask for the start level index.
    constexpr PageTable(uint64_t* root, uint32_t startLevel, uint64_t rootIndexMask) :
                m_root { root }, m_startLevel { startLevel }, m_rootIndexMask { rootIndexMask } {}

    // @return The root table pointer.
    [[nodiscard]] uint64_t* GetRoot() const { return m_root; }

    // @brief Walk to the L2 entry covering @p addr.
    // @param addr        Address to resolve (VA for stage-1, IPA for stage-2).
    // @param allocOnMiss Allocate a missing intermediate table rather than stop.
    // @return Pointer to the L2 entry, or nullptr when @p allocOnMiss is false
    //         and an intermediate table is absent.
    [[nodiscard]] uint64_t* Walk(uint64_t addr, bool allocOnMiss) const;

    // @brief Allocate a zeroed 4 KiB table from the PMM.
    // @return Pointer to the fresh table. Never returns null; halts on
    //         allocation failure so a missing page does not silently
    //         produce bogus translations.
    static uint64_t* AllocTable();

    // @brief Clean a range from the data cache to PoC.
    //
    // Real hardware page-table walkers are not obliged to observe dirty cache
    // lines produced by EL2 stores before the corresponding maintenance and
    // barriers complete. QEMU often hides this class of bug.
    //
    // @return Nothing.
    static void CleanDataCacheRange(const void* addr, size_t size);
};

#endif // !__PAGE_TABLE_H__
