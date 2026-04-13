/**
 * @file pmm.h
 * @brief Physical page allocator using the buddy allocation algorithm.
 * @ingroup mm
 *
 * Manages a contiguous range of physical memory as a pool of power-of-2
 * sized blocks. Supports allocation and deallocation of order-N blocks
 * where block size = PAGE_SIZE * 2^N.
 *
 * All addresses returned are physical addresses. The allocator operates
 * entirely before the MMU is enabled and makes no use of virtual memory.
 *
 * Free lists use an intrusive singly-linked list — the next pointer is
 * stored directly inside the free page at offset 0, costing zero metadata
 * memory.
 *
 * Buddy state (free/allocated) is tracked per order using a bitmap where
 * each bit represents a buddy pair. Toggling the bit on alloc/free tells
 * us whether the buddy is free without scanning the free list.
 */
#ifndef __PMM_H__
#define __PMM_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "core/dtb/dtb.h"

/**
 * @brief Base physical page size used by the allocator.
 * @ingroup mm
 */
static constexpr uint64_t PAGE_SIZE = 0x1000;
/**
 * @brief Bit shift corresponding to @ref PAGE_SIZE.
 * @ingroup mm
 */
static constexpr uint64_t PAGE_SHIFT = 12;
/**
 * @brief Highest allocation order supported by the allocator.
 * @ingroup mm
 *
 * Order 11 corresponds to an 8 MiB contiguous physical allocation.
 */
static constexpr uint32_t MAX_ORDER = 11;
/**
 * @brief Number of free-list levels managed by the allocator.
 * @ingroup mm
 */
static constexpr uint32_t NUM_ORDERS = MAX_ORDER + 1;

/**
 * @brief Intrusive free list node stored inside each free page.
 *
 * Lives at offset 0 of the free page. Overwritten when the page is
 * allocated and the caller takes ownership of the memory.
 */
struct FreeNode {
  FreeNode* m_next;  /**< Next free block at this order, or nullptr. */
};

/**
 * @brief Buddy bitmap storage.
 * @ingroup mm
 *
 * One bit per buddy pair per order. Bit toggled on every alloc and free.
 * If the bit is 0 after toggling, both buddies are free and can merge.
 *
 * Sized for 640MB / 4KB = 163840 pages.
 * Order 0 needs 163840/2 = 81920 bits = 10240 bytes.
 * Total across all orders is bounded below that. 16KB is ample.
 */
static constexpr size_t BITMAP_BYTES = 16384;

/**
 * @brief Physical page buddy allocator.
 * @ingroup mm
 *
 * Initialised once at boot via init(). After that, allocPages() and
 * freePages() are the only interface the rest of the hypervisor needs.
 */
class BuddyAllocator {
  private:
    uint64_t  m_base;                  /**< base address to the  pool.  */
    uint64_t  m_size;                  /**< Total size of the managed pool. */
    FreeNode* m_freeLists[NUM_ORDERS]; /**< Head of free list per order.    */
    uint8_t   m_bitmap[BITMAP_BYTES];  /**< Buddy pair bitmap across all orders. */

    /**
     * @brief Compute the bitmap bit index for a given block and order.
     *
     * Each order has its own contiguous section of the bitmap. Within an
     * order, blocks are grouped into pairs and each pair shares one bit.
     *
     * @param addr   Physical address of the block.
     * @param order  Allocation order of the block.
     * @return       Flat bit index into m_bitmap.
     */
    size_t bitmapIndex(uint64_t addr, uint32_t order) const;

    /**
     * @brief Toggle the bitmap bit for a block/order pair and return
     *        the new value.
     *
     * @param addr   Physical address of the block.
     * @param order  Allocation order.
     * @return       New bit value after toggle (0 = both buddies free).
     */
    uint8_t bitmapToggle(uint64_t addr, uint32_t order);

    /**
     * @brief Push a block onto the head of the free list for @p order.
     *
     * Writes a FreeNode header into the first bytes of the block.
     *
     * @param addr   Physical address of the block to push.
     * @param order  Target free list.
     */
    void listPush(uint64_t addr, uint32_t order);

    /**
     * @brief Pop the head block from the free list for @p order.
     *
     * @param order  Source free list.
     * @return       Physical address of the popped block, or 0 if empty.
     */
    uint64_t listPop(uint32_t order);

    /**
     * @brief Remove a specific block from the free list for @p order.
     *
     * Required by the merge path to unlink the buddy before coalescing.
     * O(n) scan of the free list for this order.
     *
     * @param addr   Physical address of the block to remove.
     * @param order  Free list to search.
     * @return       True if the block was found and removed.
     */
    bool listRemove(uint64_t addr, uint32_t order);

    /**
     * @brief Compute the buddy address of a block.
     *
     * buddy = (addr - m_base) XOR (1 << order) * PAGE_SIZE + m_base
     *
     * @param addr   Physical address of the block.
     * @param order  Allocation order.
     * @return       Physical address of the buddy block.
     */
    uint64_t buddyOf(uint64_t addr, uint32_t order) const;

    /**
     * @brief Reserve a physical region by allocating all pages it covers.
     *
     * Pages covering [base, base+size) are removed from the free pool.
     * Used during init() to punch out the kernel image, TF-A, and DTB.
     *
     * @param base  Physical base of the region to reserve.
     * @param size  Size of the region in bytes.
     */
    void reserveRegion(uint64_t base, uint64_t size);
  public:
    /**
     * @brief Initialise the allocator from the firmware memory map.
     *
     * Computes the free pool as:
     *   [__uncached_space_end, memBase + memSize)
     * minus the TF-A reservation and the DTB blob.
     *
     * Must be called exactly once before any allocPages() call.
     *
     * @param map  Parsed firmware memory map from parseDtb().
     */
    void init(const MemoryMap& map);

    /**
     * @brief Allocate a physically contiguous block of 2^order pages.
     *
     * @param order  Allocation order (0 = 4KB, 1 = 8KB, ..., 11 = 8MB).
     * @return       Physical base address of the allocated block,
     *               or 0 if the allocation failed.
     */
    uint64_t allocPages(uint32_t order);

    /**
     * @brief Return a previously allocated block to the free pool.
     *
     * Merges with the buddy block if the buddy is also free, recursing
     * upward until no further merges are possible.
     *
     * @param addr   Physical base address returned by allocPages().
     * @param order  Must match the order passed to the original allocPages().
     */
    void freePages(uint64_t addr, uint32_t order);

    /**
     * @brief Print free block counts per order over UART.
     *
     * Walks every free list and reports how many blocks are available at
     * each order. Call after init() to verify reservations are correct,
     * and after alloc/free sequences to verify merge behaviour.
     */
    void dumpState() const;
};

/**
 * @brief Global physical page allocator instance used during boot and runtime.
 * @ingroup mm
 */
extern BuddyAllocator g_Allocator;

#endif // __PMM_H__
