/**
 * @file heap.cpp
 * @brief PMM-backed slab allocator with page-order fallback.
 * @ingroup mm
 *
 * Small allocations (<= 1024 bytes, or alignment <= 1024) are served from
 * size-class slabs whose metadata lives at the start of each PMM page.
 * Larger or more strictly-aligned requests are served from a contiguous
 * page-order PMM allocation with a header placed immediately before the
 * returned pointer.
 *
 * The allocator publishes itself only through the global C++17 freestanding
 * allocation/deallocation operators below. There is no malloc-style API.
 */

#include "heap.h"

#include "core/mm/pmm/pmm.h"

#include <stddef.h>
#include <stdint.h>

#if __has_include(<new>)
#include <new>
#else
namespace std {
enum class align_val_t : size_t {};
}
#endif

[[noreturn]] void hv_panic(const char* msg);

namespace {

constexpr size_t   SLAB_CLASS_COUNT  = 7;
constexpr size_t   SLAB_CLASSES[SLAB_CLASS_COUNT] = {
    16, 32, 64, 128, 256, 512, 1024,
};
constexpr size_t   MAX_SLAB_SIZE      = 1024;
constexpr size_t   DEFAULT_NEW_ALIGN  = 16;
constexpr uint64_t SLAB_MAGIC         = 0x534C41425F4D4147ULL; // "SLAB_MAG"
constexpr uint64_t LARGE_MAGIC        = 0x4C415247455F4D47ULL; // "LARGE_MG"

struct FreeSlot {
  FreeSlot* m_next;
};

struct SlabHeader {
  uint64_t    m_magic;
  uint32_t    m_classIdx;
  uint32_t    m_inUse;
  FreeSlot*   m_freeList;
  SlabHeader* m_next;
};

struct LargeHeader {
  uint64_t m_magic;
  uint32_t m_order;
  uint32_t m_userOffset;
};

bool        s_initialized {false};
SlabHeader* s_classLists[SLAB_CLASS_COUNT] = {};

size_t pickClass(size_t size, size_t align) {
  size_t need = size > align ? size : align;
  if (need > MAX_SLAB_SIZE)
    return SLAB_CLASS_COUNT;
  for (size_t i = 0; i < SLAB_CLASS_COUNT; ++i) {
    if (SLAB_CLASSES[i] >= need)
      return i;
  }
  return SLAB_CLASS_COUNT;
}

SlabHeader* newSlab(size_t classIdx) {
  uint64_t pageAddr = pmm::allocPages(0);
  if (pageAddr == 0)
    return nullptr;

  auto* hdr      = reinterpret_cast<SlabHeader*>(pageAddr);
  hdr->m_magic    = SLAB_MAGIC;
  hdr->m_classIdx = static_cast<uint32_t>(classIdx);
  hdr->m_inUse    = 0;
  hdr->m_next     = nullptr;

  size_t slotSize        = SLAB_CLASSES[classIdx];
  size_t firstSlotOffset = (sizeof(SlabHeader) + slotSize - 1) & ~(slotSize - 1);
  size_t slotCount       = (PAGE_SIZE - firstSlotOffset) / slotSize;
  auto*  base            = reinterpret_cast<uint8_t*>(pageAddr) + firstSlotOffset;

  FreeSlot* prev = nullptr;
  for (size_t i = 0; i < slotCount; ++i) {
    auto* slot   = reinterpret_cast<FreeSlot*>(base + i * slotSize);
    slot->m_next = prev;
    prev         = slot;
  }
  hdr->m_freeList = prev;
  return hdr;
}

void* allocFromSlab(size_t classIdx) {
  SlabHeader* hdr = s_classLists[classIdx];
  while (hdr != nullptr && hdr->m_freeList == nullptr)
    hdr = hdr->m_next;

  if (hdr == nullptr) {
    hdr = newSlab(classIdx);
    if (hdr == nullptr)
      return nullptr;
    hdr->m_next            = s_classLists[classIdx];
    s_classLists[classIdx] = hdr;
  }

  FreeSlot* slot   = hdr->m_freeList;
  hdr->m_freeList  = slot->m_next;
  ++hdr->m_inUse;
  return slot;
}

void freeToSlab(void* ptr) {
  uint64_t pageAddr = reinterpret_cast<uint64_t>(ptr) & ~(PAGE_SIZE - 1);
  auto*    hdr     = reinterpret_cast<SlabHeader*>(pageAddr);
  auto*    slot    = reinterpret_cast<FreeSlot*>(ptr);

  slot->m_next    = hdr->m_freeList;
  hdr->m_freeList = slot;
  --hdr->m_inUse;
}

uint32_t orderForBytes(size_t total) {
  uint32_t order = 0;
  uint64_t span  = PAGE_SIZE;
  while (span < total) {
    if (order >= MAX_ORDER)
      return MAX_ORDER + 1;
    span <<= 1;
    ++order;
  }
  return order;
}

void* allocLarge(size_t size, size_t align) {
  constexpr size_t hdrSize = sizeof(LargeHeader);

  size_t   userOffset = (hdrSize + align - 1) & ~(align - 1);
  size_t   total      = userOffset + size;
  uint32_t order      = orderForBytes(total);
  if (order > MAX_ORDER)
    return nullptr;

  uint64_t addr = pmm::allocPages(order);
  if (addr == 0)
    return nullptr;

  // Wipe the first cache line at the allocation start so the slab-magic
  // probe in deallocate cannot accidentally match leftover PMM data.
  *reinterpret_cast<uint64_t*>(addr) = 0;

  auto* userPtr = reinterpret_cast<uint8_t*>(addr) + userOffset;
  auto* hdr     = reinterpret_cast<LargeHeader*>(userPtr - hdrSize);
  hdr->m_magic      = LARGE_MAGIC;
  hdr->m_order      = order;
  hdr->m_userOffset = static_cast<uint32_t>(userOffset);
  return userPtr;
}

void freeLarge(void* ptr) {
  auto* hdr = reinterpret_cast<LargeHeader*>(
      reinterpret_cast<uint8_t*>(ptr) - sizeof(LargeHeader));
  if (hdr->m_magic != LARGE_MAGIC)
    hv_panic("[HEAP] large free: bad magic");

  uint32_t order      = hdr->m_order;
  uint32_t userOffset = hdr->m_userOffset;
  hdr->m_magic        = 0;

  uint64_t allocStart = reinterpret_cast<uint64_t>(ptr) - userOffset;
  pmm::freePages(allocStart, order);
}

void* internalAllocate(size_t size, size_t align) {
  if (!s_initialized)
    hv_panic("[HEAP] allocation before heap::init()");

  if (size == 0)
    size = 1;
  if (align < DEFAULT_NEW_ALIGN)
    align = DEFAULT_NEW_ALIGN;

  size_t classIdx = pickClass(size, align);
  if (classIdx < SLAB_CLASS_COUNT)
    return allocFromSlab(classIdx);

  return allocLarge(size, align);
}

void internalDeallocate(void* ptr) {
  if (ptr == nullptr)
    return;

  // Slab slots are never page-aligned (firstSlotOffset > 0 for every class),
  // so a page-aligned pointer can only have come from a large allocation.
  if ((reinterpret_cast<uintptr_t>(ptr) & (PAGE_SIZE - 1)) == 0) {
    freeLarge(ptr);
    return;
  }

  uint64_t pageAddr = reinterpret_cast<uint64_t>(ptr) & ~(PAGE_SIZE - 1);
  auto*    slabHdr  = reinterpret_cast<SlabHeader*>(pageAddr);
  if (slabHdr->m_magic == SLAB_MAGIC) {
    freeToSlab(ptr);
    return;
  }

  auto* largeHdr = reinterpret_cast<LargeHeader*>(
      reinterpret_cast<uint8_t*>(ptr) - sizeof(LargeHeader));
  if (largeHdr->m_magic == LARGE_MAGIC) {
    freeLarge(ptr);
    return;
  }

  hv_panic("[HEAP] free: invalid pointer (no recognised header)");
}

}

namespace hv {
namespace heap {

void init() {
  for (auto& list : s_classLists)
    list = nullptr;
  s_initialized = true;
}

}
}

#ifdef HEAP_TESTING_BUILD
namespace hv {
namespace heap {
namespace testing {
void* allocate(size_t size, size_t align) { return internalAllocate(size, align); }
void  deallocate(void* ptr)               { internalDeallocate(ptr);              }
}
}
}
#else

void* operator new(size_t size) {
  void* p = internalAllocate(size, DEFAULT_NEW_ALIGN);
  if (p == nullptr)
    hv_panic("[HEAP] operator new failed");
  return p;
}
void* operator new[](size_t size) {
  void* p = internalAllocate(size, DEFAULT_NEW_ALIGN);
  if (p == nullptr)
    hv_panic("[HEAP] operator new[] failed");
  return p;
}
void  operator delete(void* p)             noexcept { internalDeallocate(p); }
void  operator delete[](void* p)           noexcept { internalDeallocate(p); }
void  operator delete(void* p, size_t)     noexcept { internalDeallocate(p); }
void  operator delete[](void* p, size_t)   noexcept { internalDeallocate(p); }

void* operator new(size_t size, std::align_val_t a) {
  void* p = internalAllocate(size, static_cast<size_t>(a));
  if (p == nullptr)
    hv_panic("[HEAP] aligned operator new failed");
  return p;
}
void* operator new[](size_t size, std::align_val_t a) {
  void* p = internalAllocate(size, static_cast<size_t>(a));
  if (p == nullptr)
    hv_panic("[HEAP] aligned operator new[] failed");
  return p;
}
void operator delete(void* p, std::align_val_t)         noexcept { internalDeallocate(p); }
void operator delete[](void* p, std::align_val_t)       noexcept { internalDeallocate(p); }
void operator delete(void* p, size_t, std::align_val_t) noexcept { internalDeallocate(p); }
void operator delete[](void* p, size_t, std::align_val_t) noexcept { internalDeallocate(p); }

#endif // HEAP_TESTING_BUILD
