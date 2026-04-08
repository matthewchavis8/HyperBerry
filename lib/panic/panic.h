#ifndef __PANIC_H__
#define __PANIC_H__

#include "core/exceptions/exceptions.h"

[[noreturn]] void hv_panic(const char* msg, ExceptionContext& ctx);

#endif // !__PANIC_H__
