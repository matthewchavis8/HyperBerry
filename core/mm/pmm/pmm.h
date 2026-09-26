// @file pmm.h
// @brief Physical page allocator using the buddy allocation algorithm.
// @ingroup pmm
//
// Manages a contiguous range of physical memory as a pool of power-of-2
// sized blocks. Supports allocation and deallocation of order-N blocks
// where block size = PAGE_SIZE * 2^N.
//
// All addresses returned are physical addresses. The allocator operates
// entirely before the MMU is enabled and makes no use of virtual memory.
#ifndef __PMM_H__
#define __PMM_H__

#include <array>
#include <cstddef>
#include <cstdint>

#include "core/deviceTree/deviceTree.h"

static constexpr uint64_t PAGE_SIZE { 0x1000 };
static constexpr uint64_t PAGE_SHIFT { 12 };
// TODO: Move guest RAM ownership to a block-list allocator so VMs do not
// require one large contiguous host-physical allocation.
static constexpr uint32_t MAX_ORDER { 16 };
static constexpr uint32_t NUM_ORDERS { MAX_ORDER + 1 };

// @brief Buddy allocator over the physical memory pool.
// @ingroup pmm
//
// One per system. The pool starts empty; @ref SetMemoryMap() hands it the
// memory the device tree describes, the way @ref Uart::SetBase repoints the
// console once the tree is read.
class Pmm {
private:
    static constexpr uint64_t MAX_POOL_SIZE { 0x200000000ULL };

    // State bits available across every order's buddy pairs.
    static constexpr size_t BITMAP_BYTES { [] {
        const uint64_t totalPages { MAX_POOL_SIZE >> PAGE_SHIFT };
        size_t bits {};

        for (uint32_t off {}; off <= MAX_ORDER; off++) {
            bits += static_cast<size_t>(totalPages >> (off + 1));
        }

        return (bits + 7U) / 8U;
    }() };

    // Written into each free block, pointing at the next free block of its order.
    struct FreeNode {
        FreeNode* next;
    };

    uint64_t m_base {};                               // base address of RAM
    uint64_t m_size {};                               // size of the pool
    std::array<FreeNode*, NUM_ORDERS> m_freeLists {}; // free blocks per order
    std::array<uint8_t, BITMAP_BYTES> m_bitmap {};    // state bit of every buddy pair

    constexpr Pmm() = default;

    [[nodiscard]] size_t bitmapIndex(uint64_t addr, uint32_t order) const;
    uint8_t bitmapToggle(uint64_t addr, uint32_t order);
    void listPush(uint64_t addr, uint32_t order);
    uint64_t listPop(uint32_t order);
    bool listRemove(uint64_t addr, uint32_t order);
    [[nodiscard]] uint64_t buddyOf(uint64_t addr, uint32_t order) const;
    void reserveRegion(uint64_t base, uint64_t size);

public:
    // @brief The system's one physical allocator.
    //
    // Constructed on first call, empty until @ref SetMemoryMap().
    //
    // @return Reference to the single allocator instance.
    static Pmm& GetInstance();

    // @brief Rebuild the pool from a DTB memory map.
    //
    // Frees every block the map describes, then reserves the kernel, TF-A,
    // the DTB, the boot archive and the null page.
    //
    // @param map Physical memory regions discovered from the device tree.
    // @return Nothing.
    void SetMemoryMap(const MemoryMap& map);

    // @brief Allocate a power-of-2 block of physical pages.
    // @param order Block size exponent: allocates PAGE_SIZE * 2^order bytes.
    // @return Physical base address of the allocated block, or 0 on failure.
    [[nodiscard]] uint64_t AllocPages(uint32_t order);

    // @brief Return a previously allocated block to the free pool.
    // @param addr  Physical base address returned by AllocPages().
    // @param order Must match the order passed to AllocPages().
    // @return Nothing.
    void FreePages(uint64_t addr, uint32_t order);

    // @brief Print a human-readable summary of the free-list state.
    // @return Nothing.
    void DumpState() const;

    Pmm(const Pmm&) = delete;
    Pmm& operator=(const Pmm&) = delete;
    Pmm(Pmm&&) = delete;
    Pmm& operator=(Pmm&&) = delete;
    ~Pmm() = default;
};

#endif // __PMM_H__
