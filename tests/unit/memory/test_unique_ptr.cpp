/**
 * @file test_unique_ptr.cpp
 * @brief Unit tests for hv::unique_ptr.
 */

#include <gtest/gtest.h>
#include "lib/memory/unique_ptr.h"
#include "lib/utility/utility.h"

namespace {

struct DtorCounter {
  static int s_count;
  int        m_value;
  explicit DtorCounter(int v) : m_value(v) {}
  ~DtorCounter() { ++s_count; }
};
int DtorCounter::s_count = 0;

struct CountingDeleter {
  static int s_invocations;
  void operator()(DtorCounter* p) const noexcept {
    ++s_invocations;
    delete p;
  }
};
int CountingDeleter::s_invocations = 0;

class UniquePtrTest : public ::testing::Test {
 protected:
  void SetUp() override {
    DtorCounter::s_count          = 0;
    CountingDeleter::s_invocations = 0;
  }
};

}

TEST_F(UniquePtrTest, DefaultIsNull) {
  hv::unique_ptr<int> p;
  EXPECT_FALSE(static_cast<bool>(p));
  EXPECT_EQ(p.get(), nullptr);
  EXPECT_EQ(p, nullptr);
  EXPECT_EQ(nullptr, p);
}

TEST_F(UniquePtrTest, OwnsRawPointer) {
  hv::unique_ptr<int> p(new int(7));
  ASSERT_TRUE(static_cast<bool>(p));
  EXPECT_EQ(*p, 7);
}

TEST_F(UniquePtrTest, MakeUniqueConstructs) {
  auto p = hv::make_unique<DtorCounter>(11);
  ASSERT_TRUE(static_cast<bool>(p));
  EXPECT_EQ(p->m_value, 11);
}

TEST_F(UniquePtrTest, DestructorRunsDeleter) {
  {
    auto p = hv::make_unique<DtorCounter>(1);
    EXPECT_EQ(DtorCounter::s_count, 0);
  }
  EXPECT_EQ(DtorCounter::s_count, 1);
}

TEST_F(UniquePtrTest, MoveTransfersOwnership) {
  auto a = hv::make_unique<DtorCounter>(2);
  hv::unique_ptr<DtorCounter> b(hv::move(a));

  EXPECT_FALSE(static_cast<bool>(a));
  ASSERT_TRUE(static_cast<bool>(b));
  EXPECT_EQ(b->m_value, 2);
  EXPECT_EQ(DtorCounter::s_count, 0);
}

TEST_F(UniquePtrTest, MoveAssignReleasesOld) {
  auto a = hv::make_unique<DtorCounter>(3);
  auto b = hv::make_unique<DtorCounter>(4);
  b = hv::move(a);
  EXPECT_EQ(DtorCounter::s_count, 1);
  ASSERT_TRUE(static_cast<bool>(b));
  EXPECT_EQ(b->m_value, 3);
}

TEST_F(UniquePtrTest, ResetReleases) {
  auto p = hv::make_unique<DtorCounter>(5);
  p.reset();
  EXPECT_EQ(DtorCounter::s_count, 1);
  EXPECT_FALSE(static_cast<bool>(p));
}

TEST_F(UniquePtrTest, ResetReplaces) {
  auto p = hv::make_unique<DtorCounter>(5);
  p.reset(new DtorCounter(6));
  EXPECT_EQ(DtorCounter::s_count, 1);
  ASSERT_TRUE(static_cast<bool>(p));
  EXPECT_EQ(p->m_value, 6);
}

TEST_F(UniquePtrTest, ReleaseDoesNotDelete) {
  DtorCounter* raw = nullptr;
  {
    auto p = hv::make_unique<DtorCounter>(8);
    raw = p.release();
    EXPECT_FALSE(static_cast<bool>(p));
  }
  EXPECT_EQ(DtorCounter::s_count, 0);
  delete raw;
  EXPECT_EQ(DtorCounter::s_count, 1);
}

TEST_F(UniquePtrTest, SwapExchangesPointers) {
  auto a = hv::make_unique<DtorCounter>(10);
  auto b = hv::make_unique<DtorCounter>(20);
  DtorCounter* aRaw = a.get();
  DtorCounter* bRaw = b.get();
  a.swap(b);
  EXPECT_EQ(a.get(), bRaw);
  EXPECT_EQ(b.get(), aRaw);
}

TEST_F(UniquePtrTest, NullptrAssignReleases) {
  auto p = hv::make_unique<DtorCounter>(9);
  p = nullptr;
  EXPECT_EQ(DtorCounter::s_count, 1);
  EXPECT_FALSE(static_cast<bool>(p));
}

TEST_F(UniquePtrTest, EqualitySameType) {
  auto a = hv::make_unique<int>(1);
  hv::unique_ptr<int> b;
  EXPECT_NE(a, b);
  EXPECT_NE(a, nullptr);
  EXPECT_EQ(b, nullptr);
}

TEST_F(UniquePtrTest, CustomDeleterInvoked) {
  {
    hv::unique_ptr<DtorCounter, CountingDeleter> p(new DtorCounter(42));
    EXPECT_EQ(CountingDeleter::s_invocations, 0);
  }
  EXPECT_EQ(CountingDeleter::s_invocations, 1);
  EXPECT_EQ(DtorCounter::s_count, 1);
}
