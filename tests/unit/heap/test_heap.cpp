/**
 * @file test_heap.cpp
 * @brief Unit tests for the slab + large-allocation kernel heap.
 *
 * Compiled with @c HEAP_TESTING_BUILD so heap.cpp exposes
 * @c hv::heap::testing::allocate / @c deallocate instead of overriding
 * the global C++ allocation operators (which would otherwise hijack
 * GoogleTest's own dynamic allocations before @c hv::heap::init() runs).
 */

#include <gtest/gtest.h>

#include <cstdint>
#include <cstdlib>
#include <set>
#include <vector>

#include "core/mm/heap/heap.h"
#include "core/mm/pmm/pmm.h"

// Stub PMM that satisfies page-aligned allocations from host memory.
namespace pmm {

uint64_t allocPages(uint32_t order) {
  if (order > MAX_ORDER)
    return 0;
  size_t bytes = static_cast<size_t>(PAGE_SIZE) << order;
  void*  p     = std::aligned_alloc(PAGE_SIZE, bytes);
  return reinterpret_cast<uint64_t>(p);
}

void freePages(uint64_t addr, uint32_t /*order*/) {
  std::free(reinterpret_cast<void*>(addr));
}

void init(const MemoryMap& /*map*/) {}
void dumpState() {}

}

[[noreturn]] void hv_panic(const char* /*msg*/) { std::abort(); }

namespace hv {
namespace heap {
namespace testing {
void* allocate(size_t size, size_t align);
void  deallocate(void* ptr);
}
}
}

namespace {

class HeapTest : public ::testing::Test {
 protected:
  void SetUp() override {
    static bool s_initialized = false;
    if (!s_initialized) {
      hv::heap::init();
      s_initialized = true;
    }
  }
};

}

TEST_F(HeapTest, AllocateReturnsNonNull) {
  void* p = hv::heap::testing::allocate(8, 8);
  ASSERT_NE(p, nullptr);
  hv::heap::testing::deallocate(p);
}

TEST_F(HeapTest, AllocateRespectsDefaultAlignment) {
  void* p = hv::heap::testing::allocate(1, 1);
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(p) % 16, 0U);
  hv::heap::testing::deallocate(p);
}

TEST_F(HeapTest, ZeroSizeAllocationSucceeds) {
  void* p = hv::heap::testing::allocate(0, 8);
  ASSERT_NE(p, nullptr);
  hv::heap::testing::deallocate(p);
}

TEST_F(HeapTest, SlabClassesAlignToSlotSize) {
  void* p32  = hv::heap::testing::allocate(20,  32);
  void* p128 = hv::heap::testing::allocate(100, 128);
  ASSERT_NE(p32,  nullptr);
  ASSERT_NE(p128, nullptr);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(p32)  % 32,  0U);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(p128) % 128, 0U);
  hv::heap::testing::deallocate(p32);
  hv::heap::testing::deallocate(p128);
}

TEST_F(HeapTest, SlabReusesFreedSlot) {
  void* a = hv::heap::testing::allocate(48, 8);
  hv::heap::testing::deallocate(a);
  void* b = hv::heap::testing::allocate(48, 8);
  EXPECT_EQ(a, b);
  hv::heap::testing::deallocate(b);
}

TEST_F(HeapTest, ManySmallAllocationsAreDistinct) {
  std::vector<void*> ptrs;
  ptrs.reserve(500);
  for (int i = 0; i < 500; ++i) {
    void* p = hv::heap::testing::allocate(64, 8);
    ASSERT_NE(p, nullptr);
    ptrs.push_back(p);
  }
  std::set<void*> uniq(ptrs.begin(), ptrs.end());
  EXPECT_EQ(uniq.size(), ptrs.size());
  for (void* p : ptrs)
    hv::heap::testing::deallocate(p);
}

TEST_F(HeapTest, LargeAllocationFallsBackToPages) {
  void* p = hv::heap::testing::allocate(8192, 8);
  ASSERT_NE(p, nullptr);
  hv::heap::testing::deallocate(p);
}

TEST_F(HeapTest, LargeAllocationHonorsPageAlignment) {
  void* p = hv::heap::testing::allocate(64, 4096);
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(p) % 4096, 0U);
  hv::heap::testing::deallocate(p);
}

TEST_F(HeapTest, LargeAllocationHonors2KAlignment) {
  void* p = hv::heap::testing::allocate(64, 2048);
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(p) % 2048, 0U);
  hv::heap::testing::deallocate(p);
}

TEST_F(HeapTest, FreedLargeAllocationIsReusable) {
  void* a = hv::heap::testing::allocate(8192, 8);
  ASSERT_NE(a, nullptr);
  hv::heap::testing::deallocate(a);
  void* b = hv::heap::testing::allocate(8192, 8);
  ASSERT_NE(b, nullptr);
  hv::heap::testing::deallocate(b);
}

TEST_F(HeapTest, NullptrFreeIsNoop) {
  hv::heap::testing::deallocate(nullptr);
}
