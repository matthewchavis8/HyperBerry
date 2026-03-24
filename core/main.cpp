
// Stub to see if we can boot and flash the QEMU virtual
extern "C" void hmain() {
  for (;;) {
    asm volatile("wfe");
  }
}
