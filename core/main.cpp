// @file main.cpp
// @brief Hypervisor entry point.
// @ingroup core
//
// Contains hmain(), the C++ entry called from boot.S after
// EL2 initialization, BSS zeroing, and stack setup.

#include "core/mm/pmm/pmm.h"
#include "core/mm/mmu/hostMmu/hostMmu.h"
#include "core/bootLoader/bootLoader.h"
#include "core/vm/vm.h"
#include "lib/cxxrt/cxxrt.h"
#include "lib/log/log.h"
#include "lib/panic/panic.h"
#include "drivers/gic/gic.h"
#include <cstddef>
#include "deviceTree/deviceTree.h"

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
    TreeParser hostTree { dtb };
    MemoryMap memoryMap { hostTree.ParseMemoryMap() };
    Log::Println("[DTB] Succesfully parsed device tree blob");

    Log::Println("[PMM] Attempting to bring up PMM");
    Pmm::GetInstance().SetMemoryMap(memoryMap);
    Log::Println("[PMM] Successfully brought up PMM");

    Log::Println("[MM] Memory Pool Size={:x}", memoryMap.memSize);

    Log::Println("[HostMmu] Attempting to bring up host MMU");
    HostMmu::GetInstance().Enable(hostTree.GetHostMmio());
    Log::Println("[HostMmu] Successfully host MMU is brought up");

    Log::Println("[GIC] Attempting to bring up GICv2");
    Gic::Init();
    Log::Println("[GIC] Successfully brought up GICv2");

#ifdef INTEGRATION_TEST
    TestRunner::SetBootContext(memoryMap);
    TestRunner::RunAll();
#else

    Log::Println("[Guest] Attempting to load Linux guest archive");
    cpio::Archive archive { HostMmu::PaToVa(memoryMap.cpioArchiveBase), memoryMap.cpioArchiveSize };
    BootLoader loader { archive };
    GuestLayout layout {};
    if (!loader.Load(layout)) {
        Log::Println("[Guest] archive error={}", static_cast<unsigned>(archive.GetError()));
        HvPanic("[ERROR][VM] Failed to spin up Linux VM");
    }
    Log::Println("[Guest] Linux guest archive loaded");

    Vm guest;
    const char* guestName { "Linux VM" };
    TreeParser guestTree { layout.IpaToHostPa(layout.dtbIpa) };
    guest.Init(guestName,
            GUEST_IPA_BASE,
            layout.ramHostPa,
            GUEST_RAM_SIZE,
            1,
            layout.kernelIpa,
            layout.dtbIpa,
            guestTree.GetGuestMmio());

    Log::Println("[VM] Bringing up guest:{}", guest.GetName());
    Log::Println("[VM] {} Intialized", guest.GetName());
    Log::Println("[VM] Guest Kernel running");
    guest.Run();
#endif
}
