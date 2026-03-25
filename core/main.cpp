#include "../drivers/uart/uart.h"

// Stub to see if we can boot and flash the QEMU virtual
extern "C" void hmain() {
  for (;;) {
    Uart::print("I have no mouth and I must scream:(\n");
    asm volatile("wfe");
  }
}
