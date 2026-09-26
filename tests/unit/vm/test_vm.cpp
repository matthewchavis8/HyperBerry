// @file test_vm.cpp
// @brief Unit tests for Vm construction — verifies that it wires the
//        GuestMmu and Vcpu constructors and Linux x0 DTB seeding without
//        executing real hardware paths.
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

extern uint64_t gVcpuEntryCap;
extern uint64_t gVcpuSetGuestSpCap;
extern uint64_t gVcpuSetX0Cap;

// ---------------------------------------------------------------------------
// GuestMmu capture globals
// ---------------------------------------------------------------------------

static uint64_t gGuestMmuIpa { 0xDEADDEADDEADDEADULL };
static uint64_t gGuestMmuHostPa { 0xDEADDEADDEADDEADULL };
static uint64_t gGuestMmuSize { 0xDEADDEADDEADDEADULL };
static uint8_t gGuestMmuEnableVmid { 0xFF };
static uint32_t gGuestMmuWindows { 0xFFFFFFFFU };

// ---------------------------------------------------------------------------
// GuestMmu stubs
// ---------------------------------------------------------------------------

GuestMmu::GuestMmu(uint64_t ipaBase, uint64_t hostPaBase, uint64_t sizeBytes, const MmioMap& devices) :
            m_table { nullptr, 0, 0 } {
    gGuestMmuIpa = ipaBase;
    gGuestMmuHostPa = hostPaBase;
    gGuestMmuSize = sizeBytes;
    gGuestMmuWindows = devices.GetCount();
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

static VmConfig testConfig(uint8_t vmid) {
    return VmConfig { "test-vm", 0x0ULL, 0x40000000ULL, 0x200000ULL, vmid, 0x200000ULL, 0x1FF000ULL };
}

static void resetCaptures() {
    gGuestMmuIpa = 0xDEADDEADDEADDEADULL;
    gGuestMmuHostPa = 0xDEADDEADDEADDEADULL;
    gGuestMmuSize = 0xDEADDEADDEADDEADULL;
    gGuestMmuEnableVmid = 0xFF;
    gGuestMmuWindows = 0xFFFFFFFFU;
    gVcpuEntryCap = 0xDEADDEADDEADDEADULL;
    gVcpuSetGuestSpCap = 0xDEADDEADDEADDEADULL;
    gVcpuSetX0Cap = 0xDEADDEADDEADDEADULL;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST(Vm, ConstructsGuestMmuWithCorrectArgs) {
    resetCaptures();
    MmioMap devices {};
    devices.AddPages(0x09000000ULL, 0x09000000ULL, 0x1000ULL);
    Vm vm { testConfig(1), devices };

    EXPECT_EQ(gGuestMmuIpa, 0x0ULL);
    EXPECT_EQ(gGuestMmuHostPa, 0x40000000ULL);
    EXPECT_EQ(gGuestMmuSize, 0x200000ULL);
    EXPECT_EQ(gGuestMmuWindows, 1U);
}

TEST(Vm, ConstructsVcpuWithGuestEntry) {
    resetCaptures();
    Vm vm { testConfig(1), MmioMap {} };

    EXPECT_EQ(gVcpuEntryCap, 0x200000ULL);
}

TEST(Vm, SeedsLinuxDtbInX0) {
    resetCaptures();
    Vm vm { testConfig(1), MmioMap {} };

    EXPECT_EQ(gVcpuSetX0Cap, 0x1FF000ULL);
    EXPECT_EQ(gVcpuSetGuestSpCap, 0xDEADDEADDEADDEADULL);
}

TEST(Vm, RunEnablesGuestMmuWithCorrectVmid) {
    resetCaptures();
    Vm vm { testConfig(2), MmioMap {} };
    vm.Run();

    EXPECT_EQ(gGuestMmuEnableVmid, 2);
}
