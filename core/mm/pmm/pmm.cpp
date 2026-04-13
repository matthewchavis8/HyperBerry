#include "pmm.h"
#include "drivers/uart/uart.h"

#include <stddef.h>

extern uint8_t __text_start[];
extern uint8_t __uncached_space_end[];

namespace {

constexpr size_t BITMAP_BYTES = 16384;

struct FreeNode {
  FreeNode* m_next;
};

uint64_t  s_base = 0;
uint64_t  s_size = 0;
FreeNode* s_freeLists[NUM_ORDERS] = {};
uint8_t   s_bitmap[BITMAP_BYTES] = {};

size_t bitmapIndex(uint64_t addr, uint32_t order) {
  uint64_t pageIndex = (addr - s_base) >> PAGE_SHIFT;
  uint64_t pairIndex = pageIndex >> (order + 1);

  size_t   bitOffset  = 0;
  uint64_t totalPages = s_size >> PAGE_SHIFT;

  for (uint32_t off = 0; off < order; off++) {
    bitOffset += static_cast<size_t>( (totalPages >> (off + 1)) );
  }

  return bitOffset + static_cast<size_t>(pairIndex);
}

uint8_t bitmapToggle(uint64_t addr, uint32_t order) {
  size_t  bitIdx  = bitmapIndex(addr, order);
  size_t  byteIdx = bitIdx >> 3;
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

} // namespace

namespace pmm {

uint64_t allocPages(uint32_t order) {
  if (order > MAX_ORDER)
    return 0;

  uint32_t found = MAX_ORDER + 1;
  for (uint32_t o = order; o <= MAX_ORDER; o++) {
    if (s_freeLists[o] != nullptr) {
      found = o;
      break;
    }
  }

  if (found > MAX_ORDER)
    return 0;

  uint64_t addr = listPop(found);
  bitmapToggle(addr, found);

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
  for (uint32_t o = 0; o < NUM_ORDERS; o++)
    s_freeLists[o] = nullptr;
  for (size_t i = 0; i < BITMAP_BYTES; i++)
    s_bitmap[i] = 0;

  s_base = map.memBase;
  s_size = map.memSize;

  Uart::println("[MM] Initialising buddy allocator");

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
  Uart::println("[MM] Reserved: kernel");

  reserveRegion(map.atfBase, map.atfSize);
  Uart::println("[MM] Reserved: TF-A");

  reserveRegion(map.dtbBase, map.dtbSize);
  Uart::println("[MM] Reserved: DTB");

  Uart::println("[MM] Buddy allocator ready");
  dumpState();
}

void dumpState() {
  Uart::println("[MM] Free blocks per order:");
  for (uint32_t o = 0; o <= MAX_ORDER; o++) {
    uint32_t  count = 0;
    FreeNode* node  = s_freeLists[o];
    while (node != nullptr) {
      count++;
      node = node->m_next;
    }
    Uart::println("  [order] ");
    Uart::writeHex((uint64_t)o);
    Uart::println(" [size] ");
    Uart::writeHex((uint64_t)PAGE_SIZE << o);
    Uart::println(" [free] ");
    Uart::writeHex((uint64_t)count);
    Uart::println("");
  }
}

} // namespace pmm
