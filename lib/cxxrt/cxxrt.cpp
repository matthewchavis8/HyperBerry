// @file cxxrt.cpp
// @brief Minimal C++ runtime support for the freestanding build.
// @ingroup lib

#include "cxxrt.h"
#include "lib/panic/panic.h"

extern "C" {
// Bounds emitted by the .init_array block in each board's linker script.
extern void (*__init_array_start[])();
extern void (*__init_array_end[])();
}

void runGlobalConstructors() {
    for (void (**ctor)() = __init_array_start; ctor != __init_array_end; ++ctor) {
        (*ctor)();
    }
}

extern "C" {

// Referenced by objects that register a destructor. The hypervisor never
// exits, so nothing registered here is ever run.
void* __dso_handle = nullptr;

int __cxa_atexit(void (*)(void*), void*, void*) {
    return 0;
}

void __cxa_pure_virtual() {
    hv_panic("[cxxrt] pure virtual called");
}

} // extern "C"
