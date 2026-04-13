#include "pmm.h"
#include "drivers/uart/uart.h"

#include <stddef.h>

extern uint8_t __text_start[];
extern uint8_t __uncached_space_end[];

BuddyAllocator g_Allocator;

size_t BuddyAllocator::bitmapIndex(uint64_t addr, uint32_t order) const {
  uint64_t pageIndex = (addr - m_base) >> PAGE_SHIFT;
  uint64_t pairIndex = pageIndex >> (order + 1);
 
  size_t   bitOffset  = 0;
  uint64_t totalPages = m_size >> PAGE_SHIFT;

  for (uint32_t off = 0; off < order; off++) {
    bitOffset += static_cast<size_t>( (totalPages >> (off + 1)) );
  }
 
  return bitOffset + static_cast<size_t>(pairIndex);
}
 
uint8_t BuddyAllocator::bitmapToggle(uint64_t addr, uint32_t order) {
  size_t  bitIdx  = bitmapIndex(addr, order);
  size_t  byteIdx = bitIdx >> 3;
  uint8_t mask    = (uint8_t)(1u << (bitIdx & 7u));
 
  m_bitmap[byteIdx] ^= mask;
  return (m_bitmap[byteIdx] & mask) ? 1u : 0u;
}

void BuddyAllocator::listPush(uint64_t addr, uint32_t order) {
  FreeNode* node     = reinterpret_cast<FreeNode*>(addr);
  node->m_next       = m_freeLists[order];
  m_freeLists[order] = node;
}
 
uint64_t BuddyAllocator::listPop(uint32_t order) {
  FreeNode* node = m_freeLists[order];
  if (node == nullptr)
    return 0;
  m_freeLists[order] = node->m_next;
  return reinterpret_cast<uint64_t>(node);
}
 
bool BuddyAllocator::listRemove(uint64_t addr, uint32_t order) {
  FreeNode** curr = &m_freeLists[order];
  while (*curr != nullptr) {
    if (reinterpret_cast<uint64_t>(*curr) == addr) {
      *curr = (*curr)->m_next;
      return true;
    }
    curr = &(*curr)->m_next;
  }
  return false;
}
 
uint64_t BuddyAllocator::buddyOf(uint64_t addr, uint32_t order) const {
  uint64_t offset = addr - m_base;
  uint64_t size   = (uint64_t)PAGE_SIZE << order;
  return (offset ^ size) + m_base;
}
 
uint64_t BuddyAllocator::allocPages(uint32_t order) {
  if (order > MAX_ORDER)
    return 0;
 
  // Find the lowest order >= requested that has a free block.
  uint32_t found = MAX_ORDER + 1;
  for (uint32_t o = order; o <= MAX_ORDER; o++) {
    if (m_freeLists[o] != nullptr) {
      found = o;
      break;
    }
  }
 
  if (found > MAX_ORDER)
    return 0;
 
  // Pop from the found order.
  uint64_t addr = listPop(found);
  bitmapToggle(addr, found);
 
  // Split down to the requested order pushing right-half buddies.
  while (found > order) {
    found--;
    uint64_t split = addr + ((uint64_t)PAGE_SIZE << found);
    bitmapToggle(split, found);
    listPush(split, found);
  }
 
  return addr;
}
 
void BuddyAllocator::freePages(uint64_t addr, uint32_t order) {
  if (addr == 0 || order > MAX_ORDER)
    return;
 
  while (order < MAX_ORDER) {
    uint64_t buddy = buddyOf(addr, order);
 
    // Toggle bitmap — 0 means buddy is also free, merge.
    uint8_t bit = bitmapToggle(addr, order);
    if (bit != 0) {
      listPush(addr, order);
      return;
    }
 
    if (!listRemove(buddy, order)) {
      // Keep the pair marked as partially free if the list and bitmap diverge.
      bitmapToggle(addr, order);
      listPush(addr, order);
      return;
    }
 
    // Merged block starts at the lower address.
    if (buddy < addr)
      addr = buddy;
 
    order++;
  }
 
  bitmapToggle(addr, order);
  listPush(addr, order);
}
 
void BuddyAllocator::reserveRegion(uint64_t base, uint64_t size) {
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
 
    // Advance one page if nothing matched to avoid infinite loop.
    if (!removed)
      addr += PAGE_SIZE;
  }
}
 
void BuddyAllocator::init(const MemoryMap& map) {
  // Zero all state.
  m_base = 0;
  m_size = 0;
  for (uint32_t o = 0; o < NUM_ORDERS; o++)
    m_freeLists[o] = nullptr;
  for (size_t i = 0; i < BITMAP_BYTES; i++)
    m_bitmap[i] = 0;
 
  m_base = map.memBase;
  m_size = map.memSize;
 
  Uart::println("[MM] Initialising buddy allocator");
 
  // Free the entire RAM range into the allocator first.
  // Walk in MAX_ORDER sized steps for natural alignment, then handle
  // any remaining tail with smaller orders.
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
 
void BuddyAllocator::dumpState() const {
  Uart::println("[MM] Free blocks per order:");
  for (uint32_t o = 0; o <= MAX_ORDER; o++) {
    uint32_t  count = 0;
    FreeNode* node  = m_freeLists[o];
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
 
