// @file main.cpp
// @brief Hypervisor entry point.
// @ingroup core
//
// Contains hmain(), the C++ entry called from boot.S after
// EL2 initialization, BSS zeroing, and stack setup.

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

// @brief Main hypervisor entry point (called from boot.S).
// @ingroup core
//
// Uses C linkage so the assembly boot code can branch to it by name
// without C++ name mangling.
//
// @warning Must never return. The assembly boot stub has no return
//          address — falling off the end of hmain() is undefined behaviour.
extern "C" void hmain(uintptr_t dtb) {
    RunGlobalConstructors();

    Log::Println("[DTB] Attempting to parse device tree blob");
    MemoryMap memoryMap = ParseDtb(dtb);
    Log::Println("[DTB] Succesfully parsed device tree blob");
    // TODO: future, but instead of isValid panicking here I think parseDtb or the
    // memoryMap itself should fail hard, so this check can move inside somewhere.
    if (!memoryMap.isValid) HvPanic("[ERROR][DTB] Failed to parse Tree Blob");

    VerifyBspAgainstDtb(dtb);

    Log::Println("[PMM] Attempting to bring up PMM");
    pmm::Init(memoryMap);
    Log::Println("[PMM] Successfully brought up PMM");

    Log::Println("[MM] Memory Pool Size={:x}", memoryMap.memSize);

    Log::Println("[HEAP] Attempting to bring up kernel heap");
    hv::heap::Init();
    Log::Println("[HEAP] Successfully brought up kernel heap");

    Log::Println("[HostMmu] Attempting to bring up host MMU");
    HostMmu::Init(DtbHostMmio(dtb));
    Log::Println("[HostMmu] Successfully host MMU is brought up");

    Log::Println("[GIC] Attempting to bring up GICv2");
    Gic::Init();
    Log::Println("[GIC] Successfully brought up GICv2");

#ifdef INTEGRATION_TEST
    TestRunner::SetBootContext(memoryMap);
    TestRunner::RunAll();
#else

    Log::Println("[BootPkg] Attempting to load Linux guest package");
    bootpkg::LoadResult loaded = bootpkg::LoadLinuxGuest(memoryMap);
    if (!loaded.isLoaded) {
        HvPanic("[ERROR][VM] Failed to spin up Linux VM");
    }
    Log::Println("[BootPkg] Linux guest package loaded");

    Vm guest;
    const char* guestName = "Linux VM";
    guest.Init(guestName,
            loaded.guest.guestIpaBase,
            loaded.guest.guestRamHostPa,
            loaded.guest.guestRamSize,
            1,
            loaded.guest.entryIpa,
            loaded.guest.dtbIpa,
            DtbGuestMmio(loaded.guest.dtbHostPa));

    Log::Println("[VM] Bringing up guest:{}", guest.GetName());
    Log::Println("[VM] {} Intialized", guest.GetName());
    Log::Println("[VM] Guest Kernel running");
    guest.Run();
#endif
}
