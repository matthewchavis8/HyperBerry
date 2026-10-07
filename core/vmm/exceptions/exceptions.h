// @file exceptions.h
// @brief Saved EL2 exception frame and host exception handlers.
// @ingroup vmm

#ifndef __VMM_EXCEPTIONS_H__
#define __VMM_EXCEPTIONS_H__

#include <array>
#include <cstddef>
#include <cstdint>

struct alignas(16) El2ExceptionFrame {
    std::array<uint64_t, 31> x;
    uint64_t elr;
    uint64_t spsr;
    uint64_t reserved;
};

static_assert(sizeof(El2ExceptionFrame) == 272);
static_assert(alignof(El2ExceptionFrame) == 16);
static_assert(sizeof(El2ExceptionFrame::x) == 248);
static_assert(offsetof(El2ExceptionFrame, x) == 0);
static_assert(offsetof(El2ExceptionFrame, elr) == 248);
static_assert(offsetof(El2ExceptionFrame, spsr) == 256);
static_assert(offsetof(El2ExceptionFrame, reserved) == 264);

extern "C" {

[[noreturn]] void handle_el2_sync(El2ExceptionFrame& frame);
[[noreturn]] void handle_el2_irq(El2ExceptionFrame& frame);
[[noreturn]] void handle_el2_fiq(El2ExceptionFrame& frame);
[[noreturn]] void handle_el2_serror(El2ExceptionFrame& frame);
[[noreturn]] void handle_unhandled(El2ExceptionFrame& frame);
}

#endif // __VMM_EXCEPTIONS_H__
