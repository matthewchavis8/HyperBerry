// @file log.cpp
// @brief Console logging implementation.
// @ingroup lib

#include "log.h"
#include "drivers/uart/uart.h"

void Log::sink(const char ch) {
    Uart::getInstance().putc(ch);
}
