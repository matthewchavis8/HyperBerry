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
#include "lib/cxxrt/cxxrt.h"
#include "lib/log/log.h"
#include "lib/panic/panic.h"
#include "drivers/gic/gic.h"
#include "stddef.h"
#include "dtb/dtb.h"
#include "dtb/dtbVerify.h"
#include "dtb/dtbMmio.h"

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
    runGlobalConstructors();

    Log::println("[DTB] Attempting to parse device tree blob");
    MemoryMap memoryMap = parseDtb(dtb);
    Log::println("[DTB] Succesfully parsed device tree blob");
    // TODO: future, but instead of isValid panicking here I think parseDtb or the
    // memoryMap itself should fail hard, so this check can move inside somewhere.
    if (!memoryMap.isValid) hv_panic("[ERROR][DTB] Failed to parse Tree Blob");

    verifyBspAgainstDtb(dtb);

    Log::println("[PMM] Attempting to bring up PMM");
    pmm::init(memoryMap);
    Log::println("[PMM] Successfully brought up PMM");

    Log::println("[MM] Memory Pool Size={:x}", memoryMap.memSize);

    Log::println("[HEAP] Attempting to bring up kernel heap");
    hv::heap::init();
    Log::println("[HEAP] Successfully brought up kernel heap");

    Log::println("[HostMmu] Attempting to bring up host MMU");
    HostMmu::init(dtbHostMmio(dtb));
    Log::println("[HostMmu] Successfully host MMU is brought up");

    Log::println("[GIC] Attempting to bring up GICv2");
    Gic::init();
    Log::println("[GIC] Successfully brought up GICv2");

#ifdef INTEGRATION_TEST
    TestRunner::setBootContext(memoryMap);
    TestRunner::run_all();
#else

    Log::println("[BootPkg] Attempting to load Linux guest package");
    bootpkg::LoadResult loaded = bootpkg::loadLinuxGuest(memoryMap);
    if (!loaded.isLoaded) {
        hv_panic("[ERROR][VM] Failed to spin up Linux VM");
    }
    Log::println("[BootPkg] Linux guest package loaded");

    Vm guest;
    const char* guestName = "Linux VM";
    guest.init(guestName,
            loaded.guest.guestIpaBase,
            loaded.guest.guestRamHostPa,
            loaded.guest.guestRamSize,
            1,
            loaded.guest.entryIpa,
            loaded.guest.dtbIpa,
            dtbGuestMmio(loaded.guest.dtbHostPa));

    Log::println("[VM] Bringing up guest:{}", guest.getName());
    Log::println("[VM] {} Intialized", guest.getName());
    Log::println("[VM] Guest Kernel running");
    guest.run();
#endif
}
