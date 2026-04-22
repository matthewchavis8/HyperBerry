/**
 * @file pmm.cpp
 * @brief Buddy-based physical memory allocator implementation.
 * @ingroup mm
 */

#include "pmm.h"
#include "drivers/uart/uart.h"

#include <stddef.h>

extern uint8_t __text_start[];
extern uint8_t __uncached_space_end[];

namespace {

constexpr uint64_t MAX_POOL_SIZE = 0x200000000ULL;

constexpr size_t bitmapBytesForPool(uint64_t poolSizeBytes) {
  const uint64_t totalPages = poolSizeBytes >> PAGE_SHIFT;

  size_t bits {};
  for (uint32_t off = 0; off <= MAX_ORDER; off++) {
    bits += static_cast<size_t>(totalPages >> (off + 1));
  }

  return (bits + 7U) / 8U;
}

// Get the number of state bits available in the block splits
constexpr size_t BITMAP_BYTES = bitmapBytesForPool(MAX_POOL_SIZE);

// Here we write into each page a pointer to the next page
struct FreeNode {
  FreeNode* m_next;
};

uint64_t  s_base {};                    // Base address of RAM
uint64_t  s_size {};                    // Max size of the Pool
FreeNode* s_freeLists[NUM_ORDERS] = {}; // Array of order buckets representing every page in that order
uint8_t   s_bitmap[BITMAP_BYTES] = {};  // Stores the state bits of every buddy pair

size_t bitmapIndex(uint64_t addr, uint32_t order) {
  uint64_t pageIndex = (addr - s_base) >> PAGE_SHIFT;
  uint64_t pairIndex = pageIndex >> (order + 1);

  size_t   bitOffset {};
  uint64_t totalPages = s_size >> PAGE_SHIFT;

  for (uint32_t off = 0; off < order; off++) {
    bitOffset += static_cast<size_t>( (totalPages >> (off + 1)) );
  }

  return bitOffset + static_cast<size_t>(pairIndex);
}

uint8_t bitmapToggle(uint64_t addr, uint32_t order) {
  size_t  bitIdx  = bitmapIndex(addr, order);
  size_t  byteIdx = bitIdx >> 3;
  if (byteIdx >= BITMAP_BYTES) {
    Uart::println("[ERROR] PMM bitmap overflow");
    for (;;) asm volatile("wfe");
  }
  uint8_t mask    = (uint8_t)(1u << (bitIdx & 7u));

  s_bitmap[byteIdx] ^= mask;
  return (s_bitmap[byteIdx] & mask) ? 1u : 0u;
}

void listPush(uint64_t addr, uint32_t order) {
  FreeNode* node     = reinterpret_cast<FreeNode*>(addr);
  node->m_next       = s_freeLists[order];
  s_freeLists[order] = node;
}

uint64_t listPop(uint32_t order) {
  FreeNode* node = s_freeLists[order];
  if (node == nullptr)
    return 0;
  s_freeLists[order] = node->m_next;
  return reinterpret_cast<uint64_t>(node);
}

bool listRemove(uint64_t addr, uint32_t order) {
  FreeNode** curr = &s_freeLists[order];
  while (*curr != nullptr) {
    if (reinterpret_cast<uint64_t>(*curr) == addr) {
      *curr = (*curr)->m_next;
      return true;
    }
    curr = &(*curr)->m_next;
  }
  return false;
}

uint64_t buddyOf(uint64_t addr, uint32_t order) {
  uint64_t offset = addr - s_base;
  uint64_t size   = (uint64_t)PAGE_SIZE << order;
  return (offset ^ size) + s_base;
}

void reserveRegion(uint64_t base, uint64_t size) {
  uint64_t start = base & ~(PAGE_SIZE - 1);
  uint64_t end   = (base + size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
  uint64_t addr  = start;

  while (addr < end) {
    bool removed = false;
    for (int32_t o = (int32_t)MAX_ORDER; o >= 0; o--) {
      uint64_t blockSize = (uint64_t)PAGE_SIZE << o;

      if ((addr & (blockSize - 1)) != 0)
        continue;
      if (addr + blockSize > end)
        continue;

      if (listRemove(addr, (uint32_t)o)) {
        bitmapToggle(addr, (uint32_t)o);
        addr   += blockSize;
        removed = true;
        break;
      }
    }

    if (!removed)
      addr += PAGE_SIZE;
  }
}

}

namespace pmm {

uint64_t allocPages(uint32_t order) {
  if (order > MAX_ORDER)
    return 0;

  // Find the order level
  uint32_t found = MAX_ORDER + 1;
  for (uint32_t o = order; o <= MAX_ORDER; o++) {
    if (s_freeLists[o] != nullptr) {
      found = o;
      break;
    }
  }

  // We found a buddy block that fits the request so allocate it
  uint64_t addr = listPop(found);
  bitmapToggle(addr, found);

  // Buddy block we found is too big so split it until it fits
  while (found > order) {
    found--;
    uint64_t split = addr + ((uint64_t)PAGE_SIZE << found);
    bitmapToggle(split, found);
    listPush(split, found);
  }

  return addr;
}

void freePages(uint64_t addr, uint32_t order) {
  if (addr == 0 || order > MAX_ORDER)
    return;

  while (order < MAX_ORDER) {
    uint64_t buddy = buddyOf(addr, order);

    uint8_t bit = bitmapToggle(addr, order);
    if (bit != 0) {
      listPush(addr, order);
      return;
    }

    if (!listRemove(buddy, order)) {
      bitmapToggle(addr, order);
      listPush(addr, order);
      return;
    }

    if (buddy < addr)
      addr = buddy;

    order++;
  }

  bitmapToggle(addr, order);
  listPush(addr, order);
}

void init(const MemoryMap& map) {
  s_base = 0;
  s_size = 0;

  for (auto& orderLevel : s_freeLists)
    orderLevel = nullptr;
  for (auto buddyBit : s_bitmap)
    buddyBit = 0;

  s_base = map.memBase;
  s_size = map.memSize;

  if (s_size > MAX_POOL_SIZE) {
    Uart::println("[PMM][ERROR] PMM pool larger than supported bitmap");
    for (;;) asm volatile("wfe");
  }

  Uart::println("[PMM] Initialising buddy allocator");

  uint64_t maxBlockSize = (uint64_t)PAGE_SIZE << MAX_ORDER;
  uint64_t poolEnd      = map.memBase + map.memSize;

  for (uint64_t addr = map.memBase; addr < poolEnd; ) {
    uint64_t remaining = poolEnd - addr;

    if (remaining >= maxBlockSize) {
      bitmapToggle(addr, MAX_ORDER);
      listPush(addr, MAX_ORDER);
      addr += maxBlockSize;
    } else {
      for (int32_t o = (int32_t)MAX_ORDER; o >= 0; o--) {
        uint64_t blockSize = (uint64_t)PAGE_SIZE << o;
        if (remaining >= blockSize) {
          bitmapToggle(addr, (uint32_t)o);
          listPush(addr, (uint32_t)o);
          addr      += blockSize;
          remaining -= blockSize;
        }
      }
    }
  }

  uint64_t kernelBase = reinterpret_cast<uint64_t>(__text_start);
  uint64_t kernelSize = reinterpret_cast<uint64_t>(__uncached_space_end)
                      - kernelBase;
  reserveRegion(kernelBase, kernelSize);
  Uart::println("[PMM] Reserved: kernel");

  reserveRegion(map.atfBase, map.atfSize);
  Uart::println("[PMM] Reserved: TF-A");

  reserveRegion(map.dtbBase, map.dtbSize);
  Uart::println("[PMM] Reserved: DTB");

  if (map.memBase == 0 && map.memSize >= PAGE_SIZE) {
    reserveRegion(0, PAGE_SIZE);
    Uart::println("[PMM] Reserved: null page");
  }

  Uart::println("[PMM] Buddy allocator ready");
  dumpState();
}

void dumpState() {
  Uart::println("[PMM] Free blocks per order:");
  for (uint32_t o = 0; o <= MAX_ORDER; o++) {
    uint32_t  count {};
    FreeNode* node  = s_freeLists[o];
    while (node != nullptr) {
      count++;
      node = node->m_next;
    }
    Uart::println("  [order] {} [size] {:x} [free] {}", o, (uint64_t)PAGE_SIZE << o, count);
  }
}

} // namespace pmm
