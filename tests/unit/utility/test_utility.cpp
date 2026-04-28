/**
 * @file test_utility.cpp
 * @brief Unit tests for hv::move, hv::forward, hv::swap.
 */

#include <gtest/gtest.h>
#include "lib/utility/utility.h"

namespace {

struct MoveTracker {
  int  m_value      {0};
  bool m_movedFrom  {false};

  MoveTracker() = default;
  explicit MoveTracker(int v) : m_value(v) {}

  MoveTracker(const MoveTracker& other) : m_value(other.m_value) {}
  MoveTracker(MoveTracker&& other) noexcept
      : m_value(other.m_value) {
    other.m_movedFrom = true;
    other.m_value     = 0;
  }
  MoveTracker& operator=(const MoveTracker& other) {
    m_value = other.m_value;
    return *this;
  }
  MoveTracker& operator=(MoveTracker&& other) noexcept {
    m_value           = other.m_value;
    other.m_movedFrom = true;
    other.m_value     = 0;
    return *this;
  }
};

template <typename T>
bool tookByLvalueRef(T& /*unused*/)  { return true;  }
template <typename T>
bool tookByLvalueRef(T&& /*unused*/) { return false; }

template <typename T>
bool forwardCheck(T&& v) {
  return tookByLvalueRef(hv::forward<T>(v));
}

}

TEST(HvUtility, MoveCastsToRvalueRef) {
  MoveTracker a(42);
  MoveTracker b(hv::move(a));

  EXPECT_EQ(b.m_value, 42);
  EXPECT_TRUE(a.m_movedFrom);
  EXPECT_EQ(a.m_value, 0);
}

TEST(HvUtility, ForwardPreservesLvalue) {
  int x = 1;
  EXPECT_TRUE(forwardCheck<int&>(x));
}

TEST(HvUtility, ForwardPreservesRvalue) {
  EXPECT_FALSE(forwardCheck<int>(7));
}

TEST(HvUtility, SwapExchangesValues) {
  MoveTracker a(1);
  MoveTracker b(2);
  hv::swap(a, b);

  EXPECT_EQ(a.m_value, 2);
  EXPECT_EQ(b.m_value, 1);
}
