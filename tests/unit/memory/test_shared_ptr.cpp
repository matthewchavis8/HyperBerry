/**
 * @file test_shared_ptr.cpp
 * @brief Unit tests for hv::shared_ptr.
 */

#include <gtest/gtest.h>
#include "lib/memory/shared_ptr.h"
#include "lib/utility/utility.h"

namespace {

struct DtorCounter {
  static int s_count;
  int        m_value;
  explicit DtorCounter(int v) : m_value(v) {}
  ~DtorCounter() { ++s_count; }
};
int DtorCounter::s_count = 0;

class SharedPtrTest : public ::testing::Test {
 protected:
  void SetUp() override { DtorCounter::s_count = 0; }
};

}

TEST_F(SharedPtrTest, DefaultIsEmpty) {
  hv::shared_ptr<int> p;
  EXPECT_FALSE(static_cast<bool>(p));
  EXPECT_EQ(p.get(), nullptr);
  EXPECT_EQ(p.use_count(), 0u);
  EXPECT_EQ(p, nullptr);
}

TEST_F(SharedPtrTest, MakeSharedConstructs) {
  auto p = hv::make_shared<DtorCounter>(7);
  ASSERT_TRUE(static_cast<bool>(p));
  EXPECT_EQ(p->m_value, 7);
  EXPECT_EQ(p.use_count(), 1u);
}

TEST_F(SharedPtrTest, CopyIncrementsUseCount) {
  auto a = hv::make_shared<DtorCounter>(1);
  auto b = a;
  EXPECT_EQ(a.use_count(), 2u);
  EXPECT_EQ(b.use_count(), 2u);
  EXPECT_EQ(a.get(), b.get());
}

TEST_F(SharedPtrTest, MoveTransfersOwnership) {
  auto a = hv::make_shared<DtorCounter>(2);
  auto b = hv::move(a);
  EXPECT_EQ(b.use_count(), 1u);
  EXPECT_FALSE(static_cast<bool>(a));
}

TEST_F(SharedPtrTest, DestroysOnLastRef) {
  {
    auto a = hv::make_shared<DtorCounter>(3);
    {
      auto b = a;
      EXPECT_EQ(DtorCounter::s_count, 0);
    }
    EXPECT_EQ(DtorCounter::s_count, 0);
  }
  EXPECT_EQ(DtorCounter::s_count, 1);
}

TEST_F(SharedPtrTest, ResetDropsRef) {
  auto a = hv::make_shared<DtorCounter>(4);
  auto b = a;
  a.reset();
  EXPECT_EQ(DtorCounter::s_count, 0);
  EXPECT_EQ(b.use_count(), 1u);
  b.reset();
  EXPECT_EQ(DtorCounter::s_count, 1);
}

TEST_F(SharedPtrTest, CopyAssignReleasesOld) {
  auto a = hv::make_shared<DtorCounter>(5);
  auto b = hv::make_shared<DtorCounter>(6);
  b = a;
  EXPECT_EQ(DtorCounter::s_count, 1);
  EXPECT_EQ(a.use_count(), 2u);
  EXPECT_EQ(a.get(), b.get());
}

TEST_F(SharedPtrTest, MoveAssignReleasesOld) {
  auto a = hv::make_shared<DtorCounter>(7);
  auto b = hv::make_shared<DtorCounter>(8);
  b = hv::move(a);
  EXPECT_EQ(DtorCounter::s_count, 1);
  EXPECT_EQ(b.use_count(), 1u);
  EXPECT_FALSE(static_cast<bool>(a));
}

TEST_F(SharedPtrTest, EqualitySameType) {
  auto a = hv::make_shared<int>(1);
  auto b = a;
  hv::shared_ptr<int> c;
  EXPECT_EQ(a, b);
  EXPECT_NE(a, c);
  EXPECT_EQ(c, nullptr);
  EXPECT_NE(a, nullptr);
}

TEST_F(SharedPtrTest, NullCopyShareEmpty) {
  hv::shared_ptr<int> a;
  auto b = a;
  EXPECT_EQ(a.use_count(), 0u);
  EXPECT_EQ(b.use_count(), 0u);
}
