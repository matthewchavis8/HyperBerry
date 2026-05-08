/**
 * @file test_vm.cpp
 * @brief Unit tests for Vm::init composition — verifies that init correctly
 *        wires GuestMmu::init, Vcpu::init, and Linux x0 DTB seeding
 *        without executing real hardware paths.
 *
 * Stubs for GuestMmu capture call arguments into file-scope globals. Vcpu
 * stubs live in test_vcpu.cpp (single-binary constraint); the capture globals
 * defined there are referenced here via extern declarations.
 *
 * Uart stubs (print/println/writeHex) are provided by test_dtb.cpp, which
 * is compiled into the same binary. vcpu_enter is stubbed here.
 */

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

static uint64_t gGuestMmuInitIpa      = 0xDEADDEADDEADDEADULL;
static uint64_t gGuestMmuInitHostPa   = 0xDEADDEADDEADDEADULL;
static uint64_t gGuestMmuInitSize     = 0xDEADDEADDEADDEADULL;
static uint8_t  gGuestMmuEnableVmid   = 0xFF;

// ---------------------------------------------------------------------------
// GuestMmu stubs
// ---------------------------------------------------------------------------

void GuestMmu::init(uint64_t ipaBase, uint64_t hostPaBase, uint64_t sizeBytes) { // NOLINT(readability-convert-member-functions-to-static)
  gGuestMmuInitIpa    = ipaBase;
  gGuestMmuInitHostPa = hostPaBase;
  gGuestMmuInitSize   = sizeBytes;
}

void GuestMmu::enable(uint8_t vmid) { // NOLINT(readability-convert-member-functions-to-static)
  gGuestMmuEnableVmid = vmid;
}

void GuestMmu::mapBlock(uint64_t /*ipa*/, uint64_t /*pa*/, bool /*isDevice*/) {}

void GuestMmu::tlbFlushAllGuest() {}

// ---------------------------------------------------------------------------
// pmm stub
// ---------------------------------------------------------------------------

// vcpu_enter stub (extern "C", called by Vm::run())

extern "C" void vcpu_enter(Vcpu* /*vcpu*/) {}

// ---------------------------------------------------------------------------
// Helper — reset all captures to sentinel values before each test.
// ---------------------------------------------------------------------------

static void resetCaptures() {
  gGuestMmuInitIpa    = 0xDEADDEADDEADDEADULL;
  gGuestMmuInitHostPa = 0xDEADDEADDEADDEADULL;
  gGuestMmuInitSize   = 0xDEADDEADDEADDEADULL;
  gGuestMmuEnableVmid = 0xFF;
  gVcpuInitEntryCap   = 0xDEADDEADDEADDEADULL;
  gVcpuSetGuestSpCap  = 0xDEADDEADDEADDEADULL;
  gVcpuSetX0Cap       = 0xDEADDEADDEADDEADULL;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST(Vm, InitCallsGuestMmuInitWithCorrectArgs) {
  resetCaptures();
  Vm vm;
  vm.init("test-vm", 0x0ULL, 0x40000000ULL, 0x200000ULL, 1,
          0x200000ULL, 0x1FF000ULL);

  EXPECT_EQ(gGuestMmuInitIpa,  0x0ULL);
  EXPECT_EQ(gGuestMmuInitHostPa, 0x40000000ULL);
  EXPECT_EQ(gGuestMmuInitSize, 0x200000ULL);
}

TEST(Vm, InitCallsVcpuInitWithGuestEntry) {
  resetCaptures();
  Vm vm;
  vm.init("test-vm", 0x0ULL, 0x40000000ULL, 0x200000ULL, 1,
          0x200000ULL, 0x1FF000ULL);

  EXPECT_EQ(gVcpuInitEntryCap, 0x200000ULL);
}

TEST(Vm, InitSeedsLinuxDtbInX0) {
  resetCaptures();
  Vm vm;
  vm.init("test-vm", 0x0ULL, 0x40000000ULL, 0x200000ULL, 1,
          0x200000ULL, 0x1FF000ULL);

  EXPECT_EQ(gVcpuSetX0Cap, 0x1FF000ULL);
  EXPECT_EQ(gVcpuSetGuestSpCap, 0xDEADDEADDEADDEADULL);
}

TEST(Vm, RunEnablesGuestMmuWithCorrectVmid) {
  resetCaptures();
  Vm vm;
  vm.init("test-vm", 0x0ULL, 0x40000000ULL, 0x200000ULL, 2,
          0x200000ULL, 0x1FF000ULL);
  vm.run();

  EXPECT_EQ(gGuestMmuEnableVmid, 2);
}
