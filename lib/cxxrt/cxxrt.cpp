// @file cxxrt.cpp
// @brief Minimal C++ runtime support for the freestanding build.
// @ingroup lib

#include "cxxrt.h"

extern "C" {
// Bounds emitted by the .init_array block in each board's linker script.
extern void (*__init_array_start[])();
extern void (*__init_array_end[])();
}

void RunGlobalConstructors() {
    for (void (**ctor)() = __init_array_start; ctor != __init_array_end; ++ctor) {
        (*ctor)();
    }
}
