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
  void     init(const MemoryMap& map);
  uint64_t allocPages(uint32_t order);
  void     freePages(uint64_t addr, uint32_t order);
  void     dumpState();
}

#endif // __PMM_H__
