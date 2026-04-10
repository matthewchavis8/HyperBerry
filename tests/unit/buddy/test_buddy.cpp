#include <gtest/gtest.h>
#include "core/mm/buddy/buddy.h"

#include <cstdlib>
#include <cstring>
#include <vector>
#include <set>

static constexpr size_t POOL_SIZE = 8u * 1024 * 1024; // 8MB = one MAX_ORDER block
static constexpr size_t POOL_ALIGNMENT = PAGE_SIZE << MAX_ORDER;

class BuddyTest : public ::testing::Test {
protected:
  BuddyAllocator alloc;
  uint8_t* m_pool = nullptr;

  void SetUp() override {
    void* ptr = nullptr;
    ASSERT_EQ(posix_memalign(&ptr, POOL_ALIGNMENT, POOL_SIZE), 0);
    m_pool = static_cast<uint8_t*>(ptr);
    ASSERT_EQ(reinterpret_cast<uint64_t>(m_pool) % POOL_ALIGNMENT, 0u);
    std::memset(m_pool, 0, POOL_SIZE);

    MemoryMap map = {};
    map.memBase = reinterpret_cast<uint64_t>(m_pool);
    map.memSize = POOL_SIZE;
    map.atfBase = 0;
    map.atfSize = 0;
    map.dtbBase = 0;
    map.dtbSize = 0;
    map.isValid = true;
    alloc.init(map);
  }

  void TearDown() override {
    free(m_pool);
  }

  uint64_t poolBase() const { return reinterpret_cast<uint64_t>(m_pool); }
  uint64_t poolEnd()  const { return poolBase() + POOL_SIZE; }
};

// --- Basic allocation ---

TEST_F(BuddyTest, AllocOrder0ReturnsValid) {
  uint64_t addr = alloc.allocPages(0);
  EXPECT_NE(addr, 0u);
  EXPECT_EQ(addr % PAGE_SIZE, 0u);
  EXPECT_GE(addr, poolBase());
  EXPECT_LT(addr, poolEnd());
}

TEST_F(BuddyTest, AllocTwoDifferentAddresses) {
  uint64_t a = alloc.allocPages(0);
  uint64_t b = alloc.allocPages(0);
  ASSERT_NE(a, 0u);
  ASSERT_NE(b, 0u);
  EXPECT_NE(a, b);
}

TEST_F(BuddyTest, AllocMaxOrder) {
  uint64_t a = alloc.allocPages(MAX_ORDER);
  ASSERT_NE(a, 0u);
  EXPECT_EQ(a % (PAGE_SIZE << MAX_ORDER), 0u);

  // Pool is one MAX_ORDER block — second alloc should fail.
  EXPECT_EQ(alloc.allocPages(MAX_ORDER), 0u);
}

TEST_F(BuddyTest, AllocBeyondMaxOrderFails) {
  EXPECT_EQ(alloc.allocPages(MAX_ORDER + 1), 0u);
}

// --- Free safety ---

TEST_F(BuddyTest, FreeNullSafe) {
  alloc.freePages(0, 0); // must not crash
}

TEST_F(BuddyTest, FreeInvalidOrderSafe) {
  uint64_t a = alloc.allocPages(0);
  ASSERT_NE(a, 0u);
  alloc.freePages(a, MAX_ORDER + 1); // must not crash
}

// --- Free and realloc ---

TEST_F(BuddyTest, FreeAndRealloc) {
  uint64_t a = alloc.allocPages(0);
  ASSERT_NE(a, 0u);
  alloc.freePages(a, 0);

  uint64_t b = alloc.allocPages(0);
  EXPECT_EQ(a, b);
}

// --- Coalescing ---

TEST_F(BuddyTest, BuddyCoalescing) {
  // Exhaust the pool at order 0.
  std::vector<uint64_t> pages;
  uint64_t p;
  while ((p = alloc.allocPages(0)) != 0) {
    pages.push_back(p);
  }

  // Pool exhausted — order-1 should also fail.
  EXPECT_EQ(alloc.allocPages(1), 0u);

  // Free the first two pages (they are buddies from the initial split).
  ASSERT_GE(pages.size(), 2u);
  alloc.freePages(pages[0], 0);
  alloc.freePages(pages[1], 0);

  // Coalescing should produce an order-1 block.
  uint64_t c = alloc.allocPages(1);
  EXPECT_NE(c, 0u);
}

// --- Exhaustion ---

TEST_F(BuddyTest, ExhaustPool) {
  size_t count = 0;
  while (alloc.allocPages(0) != 0) {
    count++;
  }

  // 8MB / 4KB = 2048 pages.
  EXPECT_EQ(count, POOL_SIZE / PAGE_SIZE);
  EXPECT_EQ(alloc.allocPages(0), 0u);
}

// --- Alignment ---

TEST_F(BuddyTest, AllocOrderAlignment) {
  for (uint32_t order = 0; order <= 4; order++) {
    uint64_t a = alloc.allocPages(order);
    ASSERT_NE(a, 0u) << "order " << order;
    EXPECT_EQ(a % (PAGE_SIZE << order), 0u) << "order " << order;
    alloc.freePages(a, order);
  }
}

// --- Full coalesce round-trip ---

TEST_F(BuddyTest, FullCoalesce) {
  // Allocate every page.
  std::vector<uint64_t> pages;
  uint64_t p;
  while ((p = alloc.allocPages(0)) != 0) {
    pages.push_back(p);
  }

  // Free all pages.
  for (uint64_t addr : pages) {
    alloc.freePages(addr, 0);
  }

  // Full coalesce should restore one MAX_ORDER block.
  uint64_t a = alloc.allocPages(MAX_ORDER);
  EXPECT_NE(a, 0u);
}
