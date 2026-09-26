// @file pmm.cpp
// @brief Buddy-based physical memory allocator implementation.
// @ingroup pmm

#include "pmm.h"
#include "lib/log/log.h"

#include <cstddef>

extern uint8_t __text_start[];
extern uint8_t __uncached_space_end[];

size_t Pmm::bitmapIndex(uint64_t addr, uint32_t order) const {
    uint64_t pageIndex { (addr - m_base) >> PAGE_SHIFT };
    uint64_t pairIndex { pageIndex >> (order + 1) };

    size_t bitOffset {};
    uint64_t totalPages { m_size >> PAGE_SHIFT };

    for (uint32_t off { 0 }; off < order; off++) {
        bitOffset += static_cast<size_t>((totalPages >> (off + 1)));
    }

    return bitOffset + static_cast<size_t>(pairIndex);
}

uint8_t Pmm::bitmapToggle(uint64_t addr, uint32_t order) {
    size_t bitIdx { bitmapIndex(addr, order) };
    size_t byteIdx { bitIdx >> 3 };
    if (byteIdx >= BITMAP_BYTES) {
        Log::Println("[ERROR] PMM bitmap overflow");
        for (;;)
            asm volatile("wfe");
    }
    uint8_t mask { (uint8_t)(1u << (bitIdx & 7u)) };

    m_bitmap[byteIdx] ^= mask;
    return (m_bitmap[byteIdx] & mask) ? 1u : 0u;
}

void Pmm::listPush(uint64_t addr, uint32_t order) {
    FreeNode* node { reinterpret_cast<FreeNode*>(addr) };
    node->next = m_freeLists[order];
    m_freeLists[order] = node;
}

uint64_t Pmm::listPop(uint32_t order) {
    FreeNode* node { m_freeLists[order] };
    if (node == nullptr) return 0;
    m_freeLists[order] = node->next;
    return reinterpret_cast<uint64_t>(node);
}

bool Pmm::listRemove(uint64_t addr, uint32_t order) {
    FreeNode** curr { &m_freeLists[order] };
    while (*curr != nullptr) {
        if (reinterpret_cast<uint64_t>(*curr) == addr) {
            *curr = (*curr)->next;
            return true;
        }
        curr = &(*curr)->next;
    }
    return false;
}

uint64_t Pmm::buddyOf(uint64_t addr, uint32_t order) const {
    uint64_t offset { addr - m_base };
    uint64_t size { (uint64_t)PAGE_SIZE << order };
    return (offset ^ size) + m_base;
}

void Pmm::reserveRegion(uint64_t base, uint64_t size) {
    uint64_t start { base & ~(PAGE_SIZE - 1) };
    uint64_t end { (base + size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1) };
    uint64_t addr { start };

    while (addr < end) {
        bool removed { false };
        for (int32_t o { (int32_t)MAX_ORDER }; o >= 0; o--) {
            uint64_t blockSize { (uint64_t)PAGE_SIZE << o };

            if ((addr & (blockSize - 1)) != 0) continue;
            if (addr + blockSize > end) continue;

            if (listRemove(addr, (uint32_t)o)) {
                bitmapToggle(addr, (uint32_t)o);
                addr += blockSize;
                removed = true;
                break;
            }
        }

        if (!removed) addr += PAGE_SIZE;
    }
}

Pmm& Pmm::GetInstance() {
    static Pmm pmm;
    return pmm;
}


uint64_t Pmm::AllocPages(uint32_t order) {
    if (order > MAX_ORDER) return 0;

    // Find the order level
    uint32_t found { MAX_ORDER + 1 };
    for (uint32_t o { order }; o <= MAX_ORDER; o++) {
        if (m_freeLists[o] != nullptr) {
            found = o;
            break;
        }
    }
    if (found > MAX_ORDER) return 0;

    // We found a buddy block that fits the request so allocate it
    uint64_t addr { listPop(found) };
    bitmapToggle(addr, found);

    // Buddy block we found is too big so split it until it fits
    while (found > order) {
        found--;
        uint64_t split { addr + ((uint64_t)PAGE_SIZE << found) };
        bitmapToggle(split, found);
        listPush(split, found);
    }

    return addr;
}

void Pmm::FreePages(uint64_t addr, uint32_t order) {
    if (addr == 0 || order > MAX_ORDER) return;

    while (order < MAX_ORDER) {
        uint64_t buddy { buddyOf(addr, order) };

        uint8_t bit { bitmapToggle(addr, order) };
        if (bit != 0) {
            listPush(addr, order);
            return;
        }

        if (!listRemove(buddy, order)) {
            bitmapToggle(addr, order);
            listPush(addr, order);
            return;
        }

        if (buddy < addr) addr = buddy;

        order++;
    }

    bitmapToggle(addr, order);
    listPush(addr, order);
}

void Pmm::SetMemoryMap(const MemoryMap& map) {
    m_base = 0;
    m_size = 0;

    m_freeLists.fill(nullptr);
    m_bitmap.fill(0);

    m_base = map.memBase;
    m_size = map.memSize;

    if (m_size > MAX_POOL_SIZE) {
        Log::Println("[PMM][ERROR] PMM pool larger than supported bitmap");
        for (;;)
            asm volatile("wfe");
    }

    Log::Println("[PMM] Initialising buddy allocator");

    uint64_t maxBlockSize { (uint64_t)PAGE_SIZE << MAX_ORDER };
    uint64_t poolEnd { map.memBase + map.memSize };

    for (uint64_t addr { map.memBase }; addr < poolEnd;) {
        uint64_t remaining { poolEnd - addr };

        if (remaining >= maxBlockSize) {
            bitmapToggle(addr, MAX_ORDER);
            listPush(addr, MAX_ORDER);
            addr += maxBlockSize;
        } else {
            for (int32_t o { (int32_t)MAX_ORDER }; o >= 0; o--) {
                uint64_t blockSize { (uint64_t)PAGE_SIZE << o };
                if (remaining >= blockSize) {
                    bitmapToggle(addr, (uint32_t)o);
                    listPush(addr, (uint32_t)o);
                    addr += blockSize;
                    remaining -= blockSize;
                }
            }
        }
    }

    uint64_t kernelBase { reinterpret_cast<uint64_t>(__text_start) };
    uint64_t kernelSize { reinterpret_cast<uint64_t>(__uncached_space_end) - kernelBase };
    reserveRegion(kernelBase, kernelSize);
    Log::Println("[PMM] Reserved: kernel");

    reserveRegion(map.atfBase, map.atfSize);
    Log::Println("[PMM] Reserved: TF-A");

    reserveRegion(map.dtbBase, map.dtbSize);
    Log::Println("[PMM] Reserved: DTB");

    if (map.cpioArchiveSize != 0) {
        reserveRegion(map.cpioArchiveBase, map.cpioArchiveSize);
        Log::Println("[PMM] Reserved: boot archive");
    }

    if (map.memBase == 0 && map.memSize >= PAGE_SIZE) {
        reserveRegion(0, PAGE_SIZE);
        Log::Println("[PMM] Reserved: null page");
    }

    Log::Println("[PMM] Buddy allocator ready");
    DumpState();
}

void Pmm::DumpState() const {
    Log::Println("[PMM] Free blocks per order:");
    for (uint32_t o { 0 }; o <= MAX_ORDER; o++) {
        uint32_t count {};
        FreeNode* node { m_freeLists[o] };
        while (node != nullptr) {
            count++;
            node = node->next;
        }
        Log::Println("  [order] {} [size] {:x} [free] {}", o, (uint64_t)PAGE_SIZE << o, count);
    }
}

