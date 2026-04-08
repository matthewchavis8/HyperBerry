#include "strings.h"

extern "C" void* memcpy(void* dest, const void* src, size_t n) {
  auto* d = static_cast<uint8_t*>(dest);
  const auto* s = static_cast<const uint8_t*>(src);

  for (size_t i = 0; i < n; i++) {
    d[i] = s[i];
  }

  return dest;
}

extern "C" void* memset(void* dest, int c, size_t n) {
  auto* d = static_cast<uint8_t*>(dest);

  for (size_t i = 0; i < n; i++) {
    d[i] = static_cast<uint8_t>(c);
  }

  return dest;
}
