// @file heap.cpp
// @brief PMM-backed slab allocator with page-order fallback.
// @ingroup mm
//
// The allocator publishes itself only through the global C++17 freestanding
// allocation/deallocation operators below. There is no malloc-style API.

#include "heap.h"

#include "core/mm/pmm/pmm.h"

#include <cstddef>
#include <cstdint>

#if __has_include(<new>)
#include <new>
#else
namespace std {
enum class align_val_t : size_t {};
}
#endif

[[noreturn]] void HvPanic(const char* msg);

namespace {

constexpr size_t MAX_SLAB_SIZE { 1024 };
constexpr size_t DEFAULT_NEW_ALIGN { 16 };
constexpr uint64_t SLAB_MAGIC { 0x534C41425F4D4147ULL };  // "SLAB_MAG"
constexpr uint64_t LARGE_MAGIC { 0x4C415247455F4D47ULL }; // "LARGE_MG"

uint32_t orderForBytes(size_t total) {
    uint32_t order { 0 };
    uint64_t span { PAGE_SIZE };
    while (span < total) {
        if (order >= MAX_ORDER) return MAX_ORDER + 1;
        span <<= 1;
        ++order;
    }
    return order;
}

} // namespace

Heap& Heap::GetInstance() {
    static Heap heap;
    return heap;
}

size_t Heap::pickClass(size_t size, size_t align) {
    size_t need { size > align ? size : align };
    if (need > MAX_SLAB_SIZE) return NO_SLAB_CLASS;
    for (size_t i { 0 }; i < SLAB_CLASSES.size(); ++i) {
        if (SLAB_CLASSES[i] >= need) return i;
    }
    return NO_SLAB_CLASS;
}

Heap::SlabHeader* Heap::newSlab(size_t classIdx) {
    uint64_t pageAddr { Pmm::GetInstance().AllocPages(0) };
    if (pageAddr == 0) return nullptr;

    auto* hdr { reinterpret_cast<SlabHeader*>(pageAddr) };
    hdr->magic = SLAB_MAGIC;
    hdr->classIdx = static_cast<uint32_t>(classIdx);
    hdr->inUse = 0;
    hdr->next = nullptr;

    size_t slotSize { SLAB_CLASSES[classIdx] };
    size_t firstSlotOffset { (sizeof(SlabHeader) + slotSize - 1) & ~(slotSize - 1) };
    size_t slotCount { (PAGE_SIZE - firstSlotOffset) / slotSize };
    auto* base { reinterpret_cast<uint8_t*>(pageAddr) + firstSlotOffset };

    FreeSlot* prev { nullptr };
    for (size_t i { 0 }; i < slotCount; ++i) {
        auto* slot { reinterpret_cast<FreeSlot*>(base + i * slotSize) };
        slot->next = prev;
        prev = slot;
    }
    hdr->freeList = prev;
    return hdr;
}

void* Heap::allocFromSlab(size_t classIdx) {
    SlabHeader* hdr { m_slabs[classIdx] };
    while (hdr != nullptr && hdr->freeList == nullptr)
        hdr = hdr->next;

    if (hdr == nullptr) {
        hdr = newSlab(classIdx);
        if (hdr == nullptr) return nullptr;
        hdr->next = m_slabs[classIdx];
        m_slabs[classIdx] = hdr;
    }

    FreeSlot* slot { hdr->freeList };
    hdr->freeList = slot->next;
    ++hdr->inUse;
    return slot;
}

void Heap::freeToSlab(void* ptr) {
    uint64_t pageAddr { reinterpret_cast<uint64_t>(ptr) & ~(PAGE_SIZE - 1) };
    auto* hdr { reinterpret_cast<SlabHeader*>(pageAddr) };
    auto* slot { reinterpret_cast<FreeSlot*>(ptr) };

    slot->next = hdr->freeList;
    hdr->freeList = slot;
    --hdr->inUse;
}

void* Heap::allocLarge(size_t size, size_t align) {
    constexpr size_t hdrSize { sizeof(LargeHeader) };

    size_t userOffset { (hdrSize + align - 1) & ~(align - 1) };
    size_t total { userOffset + size };
    uint32_t order { orderForBytes(total) };
    if (order > MAX_ORDER) return nullptr;

    uint64_t addr { Pmm::GetInstance().AllocPages(order) };
    if (addr == 0) return nullptr;

    // Wipe the first cache line at the allocation start so the slab-magic
    // probe in deallocate cannot accidentally match leftover PMM data.
    *reinterpret_cast<uint64_t*>(addr) = 0;

    auto* userPtr { reinterpret_cast<uint8_t*>(addr) + userOffset };
    auto* hdr { reinterpret_cast<LargeHeader*>(userPtr - hdrSize) };
    hdr->magic = LARGE_MAGIC;
    hdr->order = order;
    hdr->userOffset = static_cast<uint32_t>(userOffset);
    return userPtr;
}

void Heap::freeLarge(void* ptr) {
    auto* hdr { reinterpret_cast<LargeHeader*>(
            reinterpret_cast<uint8_t*>(ptr) - sizeof(LargeHeader)) };
    if (hdr->magic != LARGE_MAGIC) HvPanic("[HEAP] large free: bad magic");

    uint32_t order { hdr->order };
    uint32_t userOffset { hdr->userOffset };
    hdr->magic = 0;

    uint64_t allocStart { reinterpret_cast<uint64_t>(ptr) - userOffset };
    Pmm::GetInstance().FreePages(allocStart, order);
}

void* Heap::Allocate(size_t size, size_t align) {
    if (size == 0) size = 1;
    if (align < DEFAULT_NEW_ALIGN) align = DEFAULT_NEW_ALIGN;

    size_t classIdx { pickClass(size, align) };
    if (classIdx < NO_SLAB_CLASS) return allocFromSlab(classIdx);

    return allocLarge(size, align);
}

void Heap::Deallocate(void* ptr) {
    if (ptr == nullptr) return;

    // Slab slots are never page-aligned (firstSlotOffset > 0 for every class),
    // so a page-aligned pointer can only have come from a large allocation.
    if ((reinterpret_cast<uintptr_t>(ptr) & (PAGE_SIZE - 1)) == 0) {
        freeLarge(ptr);
        return;
    }

    uint64_t pageAddr { reinterpret_cast<uint64_t>(ptr) & ~(PAGE_SIZE - 1) };
    auto* slabHdr { reinterpret_cast<SlabHeader*>(pageAddr) };
    if (slabHdr->magic == SLAB_MAGIC) {
        freeToSlab(ptr);
        return;
    }

    auto* largeHdr { reinterpret_cast<LargeHeader*>(
            reinterpret_cast<uint8_t*>(ptr) - sizeof(LargeHeader)) };
    if (largeHdr->magic == LARGE_MAGIC) {
        freeLarge(ptr);
        return;
    }

    HvPanic("[HEAP] free: invalid pointer (no recognised header)");
}

#ifndef HEAP_TESTING_BUILD

void* operator new(size_t size) {
    void* p { Heap::GetInstance().Allocate(size, DEFAULT_NEW_ALIGN) };
    if (p == nullptr) HvPanic("[HEAP] operator new failed");
    return p;
}
void* operator new[](size_t size) {
    void* p { Heap::GetInstance().Allocate(size, DEFAULT_NEW_ALIGN) };
    if (p == nullptr) HvPanic("[HEAP] operator new[] failed");
    return p;
}
void operator delete(void* p) noexcept {
    Heap::GetInstance().Deallocate(p);
}
void operator delete[](void* p) noexcept {
    Heap::GetInstance().Deallocate(p);
}
void operator delete(void* p, size_t) noexcept {
    Heap::GetInstance().Deallocate(p);
}
void operator delete[](void* p, size_t) noexcept {
    Heap::GetInstance().Deallocate(p);
}

void* operator new(size_t size, std::align_val_t a) {
    void* p { Heap::GetInstance().Allocate(size, static_cast<size_t>(a)) };
    if (p == nullptr) HvPanic("[HEAP] aligned operator new failed");
    return p;
}
void* operator new[](size_t size, std::align_val_t a) {
    void* p { Heap::GetInstance().Allocate(size, static_cast<size_t>(a)) };
    if (p == nullptr) HvPanic("[HEAP] aligned operator new[] failed");
    return p;
}
void operator delete(void* p, std::align_val_t) noexcept {
    Heap::GetInstance().Deallocate(p);
}
void operator delete[](void* p, std::align_val_t) noexcept {
    Heap::GetInstance().Deallocate(p);
}
void operator delete(void* p, size_t, std::align_val_t) noexcept {
    Heap::GetInstance().Deallocate(p);
}
void operator delete[](void* p, size_t, std::align_val_t) noexcept {
    Heap::GetInstance().Deallocate(p);
}

#endif // HEAP_TESTING_BUILD
