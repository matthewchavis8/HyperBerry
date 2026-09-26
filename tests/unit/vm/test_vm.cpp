// @file test_vm.cpp
// @brief Unit tests for Vm::Init composition — verifies that init correctly
//        wires GuestMmu::Init, Vcpu::Init, and Linux x0 DTB seeding
//        without executing real hardware paths.
//
// Stubs for GuestMmu capture call arguments into file-scope globals. Vcpu
// stubs live in test_vcpu.cpp (single-binary constraint); the capture globals
// defined there are referenced here via extern declarations.
//
// Uart stubs (constructor, getInstance, putc) are provided by test_dtb.cpp, which
// is compiled into the same binary. vcpu_enter is stubbed here.

#include <gtest/gtest.h>

#include "core/mm/mmu/guestMmu/guestMmu.h"
#include "core/mm/pmm/pmm.h"
#include "core/vcpu/vcpu.h"
#include "core/vm/vm.h"

// ---------------------------------------------------------------------------
// Vcpu capture globals defined in test_vcpu.cpp — extern access only.
// ---------------------------------------------------------------------------

extern uint64_t gVcpuInitEntryCap;
extern uint64_t gVcpuSetGuestSpCap;
extern uint64_t gVcpuSetX0Cap;

// ---------------------------------------------------------------------------
// GuestMmu capture globals
// ---------------------------------------------------------------------------

static uint64_t gGuestMmuInitIpa { 0xDEADDEADDEADDEADULL };
static uint64_t gGuestMmuInitHostPa { 0xDEADDEADDEADDEADULL };
static uint64_t gGuestMmuInitSize { 0xDEADDEADDEADDEADULL };
static uint8_t gGuestMmuEnableVmid { 0xFF };
static uint32_t gGuestMmuInitWindows { 0xFFFFFFFFU };

// ---------------------------------------------------------------------------
// GuestMmu stubs
// ---------------------------------------------------------------------------

void GuestMmu::Init(uint64_t ipaBase,
        uint64_t hostPaBase,
        uint64_t sizeBytes,
        const MmioMap& devices) { // NOLINT(readability-convert-member-functions-to-static)
    gGuestMmuInitIpa = ipaBase;
    gGuestMmuInitHostPa = hostPaBase;
    gGuestMmuInitSize = sizeBytes;
    gGuestMmuInitWindows = devices.GetCount();
}

void GuestMmu::Enable(uint8_t vmid) { // NOLINT(readability-convert-member-functions-to-static)
    gGuestMmuEnableVmid = vmid;
}

void GuestMmu::MapBlock(uint64_t /*ipa*/, uint64_t /*pa*/, bool /*isDevice*/) {}

void GuestMmu::TlbFlushAllGuest() {}

// ---------------------------------------------------------------------------
// pmm stub
// ---------------------------------------------------------------------------

// vcpu_enter stub (extern "C", called by Vm::Run())

extern "C" void vcpu_enter(Vcpu* /*vcpu*/) {}

// ---------------------------------------------------------------------------
// Helper — reset all captures to sentinel values before each test.
// ---------------------------------------------------------------------------

static void resetCaptures() {
    gGuestMmuInitIpa = 0xDEADDEADDEADDEADULL;
    gGuestMmuInitHostPa = 0xDEADDEADDEADDEADULL;
    gGuestMmuInitSize = 0xDEADDEADDEADDEADULL;
    gGuestMmuEnableVmid = 0xFF;
    gGuestMmuInitWindows = 0xFFFFFFFFU;
    gVcpuInitEntryCap = 0xDEADDEADDEADDEADULL;
    gVcpuSetGuestSpCap = 0xDEADDEADDEADDEADULL;
    gVcpuSetX0Cap = 0xDEADDEADDEADDEADULL;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST(Vm, InitCallsGuestMmuInitWithCorrectArgs) {
    resetCaptures();
    Vm vm;
    MmioMap devices {};
    devices.AddPages(0x09000000ULL, 0x09000000ULL, 0x1000ULL);
    vm.Init("test-vm", 0x0ULL, 0x40000000ULL, 0x200000ULL, 1, 0x200000ULL, 0x1FF000ULL, devices);

    EXPECT_EQ(gGuestMmuInitIpa, 0x0ULL);
    EXPECT_EQ(gGuestMmuInitHostPa, 0x40000000ULL);
    EXPECT_EQ(gGuestMmuInitSize, 0x200000ULL);
    EXPECT_EQ(gGuestMmuInitWindows, 1U);
}

TEST(Vm, InitCallsVcpuInitWithGuestEntry) {
    resetCaptures();
    Vm vm;
    vm.Init("test-vm", 0x0ULL, 0x40000000ULL, 0x200000ULL, 1, 0x200000ULL, 0x1FF000ULL, MmioMap {});

    EXPECT_EQ(gVcpuInitEntryCap, 0x200000ULL);
}

TEST(Vm, InitSeedsLinuxDtbInX0) {
    resetCaptures();
    Vm vm;
    vm.Init("test-vm", 0x0ULL, 0x40000000ULL, 0x200000ULL, 1, 0x200000ULL, 0x1FF000ULL, MmioMap {});

    EXPECT_EQ(gVcpuSetX0Cap, 0x1FF000ULL);
    EXPECT_EQ(gVcpuSetGuestSpCap, 0xDEADDEADDEADDEADULL);
}

TEST(Vm, RunEnablesGuestMmuWithCorrectVmid) {
    resetCaptures();
    Vm vm;
    vm.Init("test-vm", 0x0ULL, 0x40000000ULL, 0x200000ULL, 2, 0x200000ULL, 0x1FF000ULL, MmioMap {});
    vm.Run();

    EXPECT_EQ(gGuestMmuEnableVmid, 2);
}
