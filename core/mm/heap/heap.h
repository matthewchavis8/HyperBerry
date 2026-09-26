// @file heap.h
// @brief PMM-backed kernel heap.
// @ingroup mm
//
// The kernel heap services dynamic C++ object allocation through the
// standard global @c new and @c delete operators. There is intentionally
// no public @c hmalloc/@c hfree surface: dynamic memory always flows
// through C++ object lifetime semantics or the smart pointer factories.
//
// The heap draws its pages from @ref Pmm, so nothing may allocate before the
// PMM has its memory map.
//
// @note The heap is not interrupt-safe in this initial implementation.
// TODO(IRQ): make heap allocation interrupt-safe once IRQ handling and
// locking policy mature.
// TODO(SMP): make slab/large allocator paths SMP-safe once HyperBerry
// enables multi-core.

#ifndef __HEAP_H__
#define __HEAP_H__

#include <array>
#include <cstddef>
#include <cstdint>

// @brief Slab allocator with a page-order fallback.
// @ingroup mm
//
// Small allocations (<= 1024 bytes, or alignment <= 1024) are served from
// size-class slabs whose metadata lives at the start of each PMM page.
// Larger or more strictly aligned requests are served from a contiguous
// page-order PMM allocation with a header placed immediately before the
// returned pointer.
class Heap {
private:
    static constexpr std::array<size_t, 7> SLAB_CLASSES { 16, 32, 64, 128, 256, 512, 1024 };
    static constexpr size_t NO_SLAB_CLASS { SLAB_CLASSES.size() };

    struct FreeSlot {
        FreeSlot* next;
    };

    // Sits at the start of every slab page.
    struct SlabHeader {
        uint64_t magic;
        uint32_t classIdx;
        uint32_t inUse;
        FreeSlot* freeList;
        SlabHeader* next;
    };

    // Sits immediately before the pointer a large allocation returns.
    struct LargeHeader {
        uint64_t magic;
        uint32_t order;
        uint32_t userOffset;
    };

    std::array<SlabHeader*, SLAB_CLASSES.size()> m_slabs {}; // slab pages per size class

    constexpr Heap() = default;

    static size_t pickClass(size_t size, size_t align);
    static SlabHeader* newSlab(size_t classIdx);
    static void* allocLarge(size_t size, size_t align);
    static void freeLarge(void* ptr);
    void* allocFromSlab(size_t classIdx);
    static void freeToSlab(void* ptr);

public:
    // @brief The system's one kernel heap.
    //
    // Constructed on first call, which is the first @c new.
    //
    // @return Reference to the single heap instance.
    static Heap& GetInstance();

    // @brief Allocate @p size bytes aligned to at least @p align.
    // @return The allocation, or nullptr when the PMM is exhausted.
    [[nodiscard]] void* Allocate(size_t size, size_t align);

    // @brief Return an allocation made by @ref Allocate().
    //
    // A null @p ptr is ignored. A pointer the heap did not hand out panics.
    //
    // @return Nothing.
    void Deallocate(void* ptr);

    Heap(const Heap&) = delete;
    Heap& operator=(const Heap&) = delete;
    Heap(Heap&&) = delete;
    Heap& operator=(Heap&&) = delete;
    ~Heap() = default;
};

#endif // __HEAP_H__
