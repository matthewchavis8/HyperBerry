#ifndef __STRINGS_H__
#define __STRINGS_H__

#include <stddef.h>
#include <stdint.h>

extern "C" {
  void* memcpy(void* dest, const void* src, size_t n);
  void* memset(void* dest, int c, size_t n);
}

#endif // !__STRINGS_H__
