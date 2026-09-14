// @file log.cpp
// @brief Console logging implementation.
// @ingroup lib

#include "log.h"
#include "drivers/uart/uart.h"

void Log::sink(const char ch) {
    Uart::getInstance().putc(ch);
}

void Log::writeCh(const char ch) {
    sink(ch);
}

void Log::println(const char* str) {
    log::detail::writeCString([](char ch) { Log::sink(ch); }, str);

    writeCh('\r');
    writeCh('\n');
}

void Log::print(const char* str) {
    log::detail::writeCString([](char ch) { Log::sink(ch); }, str);
}

void Log::writeHex(uint64_t val) {
    char buff[16];

    for (int i { 15 }; i >= 0; i--) {
        buff[i] = log::detail::hex[val & 0xF];
        val >>= 4;
    }

    for (int i {}; i < 16; i++) {
        writeCh(buff[i]);
    }
}
