/**
 * @file main.cpp
 * @brief Hypervisor entry point.
 * @ingroup core
 *
 * Contains hmain(), the C++ entry called from boot.S after
 * EL2 initialization, BSS zeroing, and stack setup.
 */

#include "core/mm/pmm/pmm.h"
#include "core/mm/heap/heap.h"
#include "core/mm/mmu/hostMmu/hostMmu.h"
#include "core/vm/vm.h"
#include "lib/panic/panic.h"
#include "uart.h"
#include "stddef.h"
#include "dtb/dtb.h"

// Minimal guest kernel: trap to the hypervisor first, then idle.
// Direct UART MMIO from EL1 is not stage-2 mapped yet.
extern "C" void guest_stub() {
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
 */
extern "C" void hmain(uintptr_t dtb) {
  Uart::init();
  Uart::println("[UART] UART intialized");

  Uart::println("[DTB] Attempting to parse device tree blob");
  MemoryMap memoryMap = parseDtb(dtb);
  Uart::println("[DTB] Succesfully parsed device tree blob");

  if (!memoryMap.isValid)
    hv_panic("[ERROR][DTB] Failed to parse Tree Blob");
 
  Uart::println("[PMM] Attempting to bring up PMM");
  pmm::init(memoryMap);
  Uart::println("[PMM] Successfully brought up PMM");

  Uart::println("[MM] Memory Pool Size={:x}", memoryMap.memSize);

  Uart::println("[HEAP] Attempting to bring up kernel heap");
  hv::heap::init();
  Uart::println("[HEAP] Successfully brought up kernel heap");

  Uart::println("[HostMmu] Attempting to bring up host MMU");
  HostMmu::init();
  Uart::println("[HostMmu] Successfully host MMU is brought up");
  
#ifdef INTEGRATION_TEST
  TestRunner::run_all();
#else

  Vm guest;
  const char* guestName = "Guest 0";
  uint64_t guestEntry = reinterpret_cast<uint64_t>(guest_stub);
  uint64_t guestIpaBase = guestEntry & ~(SIZE_1GB - 1ULL);

  Uart::println("[VM] Bringing up guest:{}", guest.getName());
  guest.init(guestName, guestIpaBase, SIZE_1GB, 1, guestEntry);
  Uart::println("[VM] {} Intialized", guest.getName());
  Uart::println("[VM] Guest Kernel running");
  guest.run();
#endif
}
