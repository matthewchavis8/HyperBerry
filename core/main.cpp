/**
 * @file main.cpp
 * @brief Hypervisor entry point.
 * @ingroup core
 *
 * Contains hmain(), the C++ entry called from boot.S after
 * EL2 initialization, BSS zeroing, and stack setup.
 */

#include "core/mm/pmm/pmm.h"
#include "core/mm/hostMmu/hostMmu.h"
#include "core/vm/vm.h"
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

  Uart::println("[HostMmu] Attempting to bring up host MMU");
  HostMmu::init();
  Uart::println("[HostMmu] Successfully host MMU is brought up");
  
#ifdef INTEGRATION_TEST
  TestRunner::run_all();
#else

  static Vm guest;
  uint64_t guestEntry = reinterpret_cast<uint64_t>(guest_stub);
  uint64_t guestIpaBase = guestEntry & ~(SIZE_1GB - 1ULL);

  guest.init(/*ipaBase=*/guestIpaBase,
             /*sizeBytes=*/SIZE_1GB,
             /*vmid=*/1,
             /*guestEntry=*/guestEntry);
  guest.run();
#endif
}
