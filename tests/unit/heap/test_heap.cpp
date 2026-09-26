// @file test_heap.cpp
// @brief Unit tests for the slab + large-allocation kernel heap.
//
// Compiled with @c HEAP_TESTING_BUILD so heap.cpp leaves the global C++
// allocation operators alone; GoogleTest keeps the host allocator and the
// tests reach the heap through @c Heap::GetInstance().

#include <gtest/gtest.h>

#include <cstdint>
#include <cstdlib>
#include <set>
#include <vector>

#include "core/mm/heap/heap.h"
#include "core/mm/pmm/pmm.h"

// Stub PMM that satisfies page-aligned allocations from host memory.
Pmm& Pmm::GetInstance() {
    static Pmm pmm;
    return pmm;
}

uint64_t Pmm::AllocPages(uint32_t order) {
    if (order > MAX_ORDER) return 0;
    size_t bytes { static_cast<size_t>(PAGE_SIZE) << order };
    void* p { std::aligned_alloc(PAGE_SIZE, bytes) };
    return reinterpret_cast<uint64_t>(p);
}

void Pmm::FreePages(uint64_t addr, uint32_t /*order*/) {
    std::free(reinterpret_cast<void*>(addr));
}

[[noreturn]] void HvPanic(const char* /*msg*/) {
    std::abort();
}

namespace {

class HeapTest : public ::testing::Test {};

} // namespace

TEST_F(HeapTest, AllocateReturnsNonNull) {
    void* p { Heap::GetInstance().Allocate(8, 8) };
    ASSERT_NE(p, nullptr);
    Heap::GetInstance().Deallocate(p);
}

TEST_F(HeapTest, AllocateRespectsDefaultAlignment) {
    void* p { Heap::GetInstance().Allocate(1, 1) };
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(p) % 16, 0U);
    Heap::GetInstance().Deallocate(p);
}

TEST_F(HeapTest, ZeroSizeAllocationSucceeds) {
    void* p { Heap::GetInstance().Allocate(0, 8) };
    ASSERT_NE(p, nullptr);
    Heap::GetInstance().Deallocate(p);
}

TEST_F(HeapTest, SlabClassesAlignToSlotSize) {
    void* p32 { Heap::GetInstance().Allocate(20, 32) };
    void* p128 { Heap::GetInstance().Allocate(100, 128) };
    ASSERT_NE(p32, nullptr);
    ASSERT_NE(p128, nullptr);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(p32) % 32, 0U);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(p128) % 128, 0U);
    Heap::GetInstance().Deallocate(p32);
    Heap::GetInstance().Deallocate(p128);
}

TEST_F(HeapTest, SlabReusesFreedSlot) {
    void* a { Heap::GetInstance().Allocate(48, 8) };
    Heap::GetInstance().Deallocate(a);
    void* b { Heap::GetInstance().Allocate(48, 8) };
    EXPECT_EQ(a, b);
    Heap::GetInstance().Deallocate(b);
}

TEST_F(HeapTest, ManySmallAllocationsAreDistinct) {
    std::vector<void*> ptrs;
    ptrs.reserve(500);
    for (int i { 0 }; i < 500; ++i) {
        void* p { Heap::GetInstance().Allocate(64, 8) };
        ASSERT_NE(p, nullptr);
        ptrs.push_back(p);
    }
    std::set<void*> uniq(ptrs.begin(), ptrs.end());
    EXPECT_EQ(uniq.size(), ptrs.size());
    for (void* p : ptrs)
        Heap::GetInstance().Deallocate(p);
}

TEST_F(HeapTest, LargeAllocationFallsBackToPages) {
    void* p { Heap::GetInstance().Allocate(8192, 8) };
    ASSERT_NE(p, nullptr);
    Heap::GetInstance().Deallocate(p);
}

TEST_F(HeapTest, LargeAllocationHonorsPageAlignment) {
    void* p { Heap::GetInstance().Allocate(64, 4096) };
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(p) % 4096, 0U);
    Heap::GetInstance().Deallocate(p);
}

TEST_F(HeapTest, LargeAllocationHonors2KAlignment) {
    void* p { Heap::GetInstance().Allocate(64, 2048) };
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(p) % 2048, 0U);
    Heap::GetInstance().Deallocate(p);
}

TEST_F(HeapTest, FreedLargeAllocationIsReusable) {
    void* a { Heap::GetInstance().Allocate(8192, 8) };
    ASSERT_NE(a, nullptr);
    Heap::GetInstance().Deallocate(a);
    void* b { Heap::GetInstance().Allocate(8192, 8) };
    ASSERT_NE(b, nullptr);
    Heap::GetInstance().Deallocate(b);
}

TEST_F(HeapTest, NullptrFreeIsNoop) {
    Heap::GetInstance().Deallocate(nullptr);
}
