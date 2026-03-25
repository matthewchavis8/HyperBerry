/**
 * @file main.cpp
 * @brief Hypervisor entry point.
 *
 * Contains hmain(), the C++ entry called from boot.S after
 * EL2 initialization, BSS zeroing, and stack setup.
 */

#include "uart.h"

/**
 * @brief Main hypervisor entry point (called from boot.S).
 *
 * Uses C linkage so the assembly boot code can branch to it by name.
 * Currently serves as a boot verification stub that prints to UART.
 */
extern "C" void hmain() {
  for (;;) {
    Uart::print("I have no mouth and I must scream:(\n");
    asm volatile("wfe");
  }
}
