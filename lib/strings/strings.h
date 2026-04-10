/**
 * @file strings.h
 * @brief Freestanding memory utility declarations.
 * @ingroup lib
 *
 * Declares the minimal C memory primitives supplied by HyperBerry when no
 * hosted C runtime is available.
 */

#ifndef __STRINGS_H__
#define __STRINGS_H__

#include <stddef.h>
#include <stdint.h>

extern "C" {
  /**
   * @brief Copy a byte range from one memory region to another.
   * @ingroup lib
   * @param dest Destination buffer.
   * @param src Source buffer.
   * @param n Number of bytes to copy.
   * @return The original @p dest pointer.
   *
   * Behaviour is undefined when the source and destination ranges overlap.
   */
  void* memcpy(void* dest, const void* src, size_t n);
  /**
   * @brief Fill a memory region with a repeated byte value.
   * @ingroup lib
   * @param dest Destination buffer.
   * @param c Byte value written to each element, converted to @c uint8_t.
   * @param n Number of bytes to write.
   * @return The original @p dest pointer.
   */
  void* memset(void* dest, int c, size_t n);
}

#endif // !__STRINGS_H__
