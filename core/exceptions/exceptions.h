#ifndef __EXCEPTIONS_H__
#define __EXCEPTIONS_H__

#include <stdint.h>
#include "lib/array.h"

struct ExceptionContext {
  hv::array<uint64_t, 30> gpr;
  uint64_t lr;
  uint64_t elr;
  uint64_t spsr;
};

#endif // __EXCEPTIONS_H__
