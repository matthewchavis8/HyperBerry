/**
 * @file main.cpp
 * @brief Hypervisor entry point.
 * @ingroup core
 *
 * Contains hmain(), the C++ entry called from boot.S after
 * EL2 initialization, BSS zeroing, and stack setup.
 */

#include "core/mm/pmm/pmm.h"
#include "core/mm/mmu/mmu.h"
#include "core/vcpu/vcpu.h"
#include "uart.h"
#include "stddef.h"
#include "dtb/dtb.h"

// Minimal guest kernel: traps to HV via HVC, then prints a message and sleeps.
extern "C" void guest_stub() {
  Uart::println("Hello World from the guest kernel!");

  asm volatile("hvc #0");

  for (;;) asm volatile("wfe");
}

#ifdef INTEGRATION_TEST
#include "tests/integration/suite.h"
#endif

/**
 * @brief Main hypervisor entry point (called from boot.S).
 * @ingroup core
 *
 * Uses C linkage so the assembly boot code can branch to it by name
 * without C++ name mangling.
 *
 * @warning Must never return. The assembly boot stub has no return
 *          address — falling off the end of hmain() is undefined behaviour.
 * @note Currently a Phase 0 boot stub. Future phases will replace the
 *       infinite loop with vCPU scheduling and Stage-2 MMU setup.
 */
extern "C" void hmain(uintptr_t dtb) {
  Uart::init();
  Uart::println("[UART] UART intialized");

  Uart::println("[DTB] Attempting to parse device tree blob");
  MemoryMap memoryMap = parseDtb(dtb);
  Uart::println("[DTB] Succesfully parsed device tree blob");

  if (!memoryMap.isValid) {
    Uart::println("[ERROR][DTB] Failed to parse Tree Blob");
    for (;;) asm volatile("wfe");
  }
 
  Uart::println("[PMM] Attempting to bring up PMM");
  pmm::init(memoryMap);
  Uart::println("[PMM] Successfully brought up PMM");
  
  Uart::println("[MM] Memory Pool Size=");
  Uart::writeHex(memoryMap.memSize);
  Uart::println("");

  Uart::println("[MMU] Attempting to bring up MMU");
  mmu::init();
  Uart::println("[MMU] Successfully MMU is brought up");
  
#ifdef INTEGRATION_TEST
  TestRunner::run_all();
#else

  static Vcpu gVcpu;
  gVcpu.init(reinterpret_cast<uint64_t>(guest_stub));

  // Guest needs a valid EL1 stack — allocate one page, point SP_EL1 at the top.
  uint64_t guestStackBase = pmm::allocPages(0);
  gVcpu.setGuestSp(guestStackBase + PAGE_SIZE);

  Uart::println("[VCPU] entering guest");
  vcpu_enter(&gVcpu);
#endif
}
