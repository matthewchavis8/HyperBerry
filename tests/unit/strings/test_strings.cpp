#include <gtest/gtest.h>
#include <cstdint>
#include <cstddef>

extern "C" {
  void* memcpy(void* dest, const void* src, size_t n);
  void* memset(void* dest, int c, size_t n);
}

TEST(Strings, MemcpyBasic) {
  const uint8_t src[] = {1, 2, 3, 4, 5, 6, 7, 8};
  uint8_t dst[8] = {};
  memcpy(dst, src, sizeof(src));
  for (int i = 0; i < 8; i++)
    EXPECT_EQ(dst[i], src[i]);
}

TEST(Strings, MemcpyZeroLength) {
  uint8_t src[4] = {0xAA, 0xBB, 0xCC, 0xDD};
  uint8_t dst[4] = {1, 2, 3, 4};
  memcpy(dst, src, 0);
  EXPECT_EQ(dst[0], 1);
  EXPECT_EQ(dst[3], 4);
}

TEST(Strings, MemcpyReturnsDestination) {
  uint8_t src[4] = {1, 2, 3, 4};
  uint8_t dst[4] = {};
  EXPECT_EQ(memcpy(dst, src, sizeof(src)), dst);
}

TEST(Strings, MemcpyLargeBlock) {
  constexpr size_t N = 4096;
  uint8_t src[N];
  uint8_t dst[N];
  for (size_t i = 0; i < N; i++)
    src[i] = static_cast<uint8_t>(i & 0xFF);
  memcpy(dst, src, N);
  for (size_t i = 0; i < N; i++)
    EXPECT_EQ(dst[i], src[i]) << "mismatch at index " << i;
}

TEST(Strings, MemsetPattern) {
  uint8_t buf[128];
  memset(buf, 0xAB, sizeof(buf));
  for (auto b : buf)
    EXPECT_EQ(b, 0xAB);
}

TEST(Strings, MemsetZero) {
  uint8_t buf[64];
  for (auto& b : buf) b = 0xFF;
  memset(buf, 0, sizeof(buf));
  for (auto b : buf)
    EXPECT_EQ(b, 0);
}

TEST(Strings, MemsetZeroLength) {
  uint8_t buf[4] = {1, 2, 3, 4};
  memset(buf, 0xFF, 0);
  EXPECT_EQ(buf[0], 1);
  EXPECT_EQ(buf[3], 4);
}

TEST(Strings, MemsetReturnsDestination) {
  uint8_t buf[4];
  EXPECT_EQ(memset(buf, 0, sizeof(buf)), buf);
}
