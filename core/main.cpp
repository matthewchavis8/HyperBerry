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
#include "core/bootpkg/bootpkg.h"
#include "core/vm/vm.h"
#include "lib/panic/panic.h"
#include "uart.h"
#include "stddef.h"
#include "dtb/dtb.h"

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

  Uart::println("[BootPkg] Attempting to load Linux guest package");
  bootpkg::LoadResult loaded = bootpkg::loadLinuxGuest(memoryMap);
  if (!loaded.isLoaded) {
    hv_panic("[ERROR][VM] Failed to spin up Linux VM");
  }
  Uart::println("[BootPkg] Linux guest package loaded");

  Vm guest;
  const char* guestName = "Linux VM";
  guest.init(guestName,
             loaded.guest.guestIpaBase,
             loaded.guest.guestRamHostPa,
             loaded.guest.guestRamSize,
             1,
             loaded.guest.entryIpa,
             loaded.guest.dtbIpa);

  Uart::println("[VM] Bringing up guest:{}", guest.getName());
  Uart::println("[VM] {} Intialized", guest.getName());
  Uart::println("[VM] Guest Kernel running");
  guest.run();
#endif
}
