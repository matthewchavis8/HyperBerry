#include <gtest/gtest.h>
#include "lib/array/array.h"

TEST(HvArray, SizeCorrect) {
  hv::array<int, 5> arr {1, 2, 3, 4, 5};
  size_t expected { 5 };

  EXPECT_EQ(arr.size(), expected);
}

TEST(HvArray, SizeZero) {
  hv::array<int, 0> arr {};
  size_t expected {};

  EXPECT_EQ(arr.size(), expected);
  EXPECT_TRUE(arr.empty());
}

TEST(HvArray, NotEmpty) {
  hv::array<int, 3> arr {};

  EXPECT_FALSE(arr.empty());
}

TEST(HvArray, BracketAccess) {
  hv::array<int, 3> arr {10, 20, 30};

  EXPECT_EQ(arr[0], 10);
  EXPECT_EQ(arr[1], 20);
  EXPECT_EQ(arr[2], 30);
}

TEST(HvArray, BracketWrite) {
  hv::array<int, 3> arr {};
  int expected { 42 };
  arr[1] = 42;

  EXPECT_EQ(arr[1], expected);
}

TEST(HvArray, FrontBack) {
  hv::array<int, 4> a = {1, 2, 3, 4};

  EXPECT_EQ(a.front(), 1);
  EXPECT_EQ(a.back(), 4);
}

TEST(HvArray, Fill) {
  hv::array<uint64_t, 4> arr {};
  arr.fill(0xDEAD);

  for (const uint64_t element : arr) {
    EXPECT_EQ(element, 0xDEAD);
  }
}

TEST(HvArray, DataPointer) {
  hv::array<int, 3> a {1, 2, 3};
  int* p = a.data();
  
  EXPECT_EQ(p, &a[0]);
  EXPECT_EQ(p[2], 3);
}

TEST(HvArray, Iterators) {
  hv::array<int, 3> a {1, 2, 3};
  int expected { 6 };
  int sum {};

  for (auto v : a) {
    sum += v;
  }

  EXPECT_EQ(sum, expected);
}

TEST(HvArray, ConstIterators) {
  const hv::array<int, 3> a {10, 20, 30};
  int expected { 60 };
  int sum {};

  for (auto it = a.cbegin(); it != a.cend(); ++it) {
    sum += *it;
  }

  EXPECT_EQ(sum, expected);
}

TEST(HvArray, MaxSize) {
  hv::array<int, 7> a {};
  size_t expected { 7 };

  EXPECT_EQ(a.max_size(), expected);
  EXPECT_EQ(a.max_size(), a.size());
}
