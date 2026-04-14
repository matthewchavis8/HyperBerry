/**
 * @file pmm.h
 * @brief Physical page allocator using the buddy allocation algorithm.
 * @ingroup pmm
 *
 * Manages a contiguous range of physical memory as a pool of power-of-2
 * sized blocks. Supports allocation and deallocation of order-N blocks
 * where block size = PAGE_SIZE * 2^N.
 *
 * All addresses returned are physical addresses. The allocator operates
 * entirely before the MMU is enabled and makes no use of virtual memory.
 */
#ifndef __PMM_H__
#define __PMM_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "core/dtb/dtb.h"

static constexpr uint64_t PAGE_SIZE  = 0x1000;
static constexpr uint64_t PAGE_SHIFT = 12;
static constexpr uint32_t MAX_ORDER  = 11;
static constexpr uint32_t NUM_ORDERS = MAX_ORDER + 1;

namespace pmm {
  /**
   * @brief Initialise the buddy allocator from a DTB memory map.
   * @ingroup pmm
   * @param map Physical memory regions discovered from the device tree.
   */
  void init(const MemoryMap& map);

  /**
   * @brief Allocate a power-of-2 block of physical pages.
   * @ingroup pmm
   * @param order Block size exponent: allocates PAGE_SIZE * 2^order bytes.
   * @return Physical base address of the allocated block, or 0 on failure.
   */
  uint64_t allocPages(uint32_t order);

  /**
   * @brief Return a previously allocated block to the free pool.
   * @ingroup pmm
   * @param addr  Physical base address returned by allocPages().
   * @param order Must match the order passed to allocPages().
   */
  void freePages(uint64_t addr, uint32_t order);

  /**
   * @brief Print a human-readable summary of the free-list state.
   * @ingroup pmm
   */
  void dumpState();
}

#endif // __PMM_H__
