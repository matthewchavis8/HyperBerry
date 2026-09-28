// @file exceptions.cpp
// @brief Fatal host exception handlers.
// @ingroup vmm

#include "exceptions.h"
#include "lib/panic/panic.h"

extern "C" void handle_el2_sync(El2ExceptionFrame& frame) {
    HvPanic("[HV sync] was triggered", frame.x);
}

extern "C" void handle_el2_irq(El2ExceptionFrame& frame) {
    HvPanic("[HV irq] was triggered", frame.x);
}

extern "C" void handle_el2_fiq(El2ExceptionFrame& frame) {
    HvPanic("[HV fiq] was triggered", frame.x);
}

extern "C" void handle_el2_serror(El2ExceptionFrame& frame) {
    HvPanic("[HV SError] was triggered", frame.x);
}

extern "C" void handle_unhandled(El2ExceptionFrame& frame) {
    HvPanic("[HV mysterious exception?] was triggered", frame.x);
}
