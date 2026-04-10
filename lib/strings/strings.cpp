/**
 * @file strings.cpp
 * @brief Freestanding memory utility implementations.
 * @ingroup lib
 */

#include "strings.h"

/**
 * @brief Copy a byte range from one memory region to another.
 * @ingroup lib
 * @param dest Destination buffer.
 * @param src Source buffer.
 * @param n Number of bytes to copy.
 * @return The original @p dest pointer.
 */
extern "C" void* memcpy(void* dest, const void* src, size_t n) {
  auto* d = static_cast<uint8_t*>(dest);
  const auto* s = static_cast<const uint8_t*>(src);

  for (size_t i = 0; i < n; i++) {
    d[i] = s[i];
  }

  return dest;
}

/**
 * @brief Fill a memory region with a repeated byte value.
 * @ingroup lib
 * @param dest Destination buffer.
 * @param c Byte value written to each byte of the destination.
 * @param n Number of bytes to write.
 * @return The original @p dest pointer.
 */
extern "C" void* memset(void* dest, int c, size_t n) {
  auto* d = static_cast<uint8_t*>(dest);

  for (size_t i = 0; i < n; i++) {
    d[i] = static_cast<uint8_t>(c);
  }

  return dest;
}
