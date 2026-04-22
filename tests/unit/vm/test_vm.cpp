/**
 * @file test_vm.cpp
 * @brief Unit tests for Vm::init composition — verifies that init correctly
 *        wires GuestMmu::init, Vcpu::init, pmm::allocPages, and
 *        Vcpu::setGuestSp without executing real hardware paths.
 *
 * Stubs for GuestMmu and pmm capture call arguments into file-scope globals.
 * Vcpu stubs live in test_vcpu.cpp (single-binary constraint); the capture
 * globals defined there are referenced here via extern declarations.
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

// ---------------------------------------------------------------------------
// GuestMmu capture globals
// ---------------------------------------------------------------------------

static uint64_t gGuestMmuInitIpa    = 0xDEADDEADDEADDEADULL;
static uint64_t gGuestMmuInitSize   = 0xDEADDEADDEADDEADULL;
static uint8_t  gGuestMmuEnableVmid = 0xFF;

// ---------------------------------------------------------------------------
// GuestMmu stubs
// ---------------------------------------------------------------------------

void GuestMmu::init(uint64_t ipaBase, uint64_t sizeBytes) { // NOLINT(readability-convert-member-functions-to-static)
  gGuestMmuInitIpa  = ipaBase;
  gGuestMmuInitSize = sizeBytes;
}

void GuestMmu::enable(uint8_t vmid) { // NOLINT(readability-convert-member-functions-to-static)
  gGuestMmuEnableVmid = vmid;
}

void GuestMmu::mapBlock(uint64_t /*ipa*/, uint64_t /*pa*/, bool /*isDevice*/) {}

void GuestMmu::tlbFlushAllGuest() {}

// ---------------------------------------------------------------------------
// pmm stub
// ---------------------------------------------------------------------------

static constexpr uint64_t kAllocPagesSentinel = 0x10000000ULL;

namespace pmm {
uint64_t allocPages(uint32_t /*order*/) {
  return kAllocPagesSentinel;
}
}  // namespace pmm

// ---------------------------------------------------------------------------
// vcpu_enter stub (extern "C", called by Vm::run())
// ---------------------------------------------------------------------------

extern "C" void vcpu_enter(Vcpu* /*vcpu*/) {}

// ---------------------------------------------------------------------------
// Helper — reset all captures to sentinel values before each test.
// ---------------------------------------------------------------------------

static void resetCaptures() {
  gGuestMmuInitIpa    = 0xDEADDEADDEADDEADULL;
  gGuestMmuInitSize   = 0xDEADDEADDEADDEADULL;
  gGuestMmuEnableVmid = 0xFF;
  gVcpuInitEntryCap   = 0xDEADDEADDEADDEADULL;
  gVcpuSetGuestSpCap  = 0xDEADDEADDEADDEADULL;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST(Vm, InitCallsGuestMmuInitWithCorrectArgs) {
  resetCaptures();
  Vm vm;
  vm.init(0x40000000ULL, 0x200000ULL, 1, 0x40000000ULL);

  EXPECT_EQ(gGuestMmuInitIpa,  0x40000000ULL);
  EXPECT_EQ(gGuestMmuInitSize, 0x200000ULL);
}

TEST(Vm, InitCallsVcpuInitWithGuestEntry) {
  resetCaptures();
  Vm vm;
  vm.init(0x40000000ULL, 0x200000ULL, 1, 0x40000000ULL);

  EXPECT_EQ(gVcpuInitEntryCap, 0x40000000ULL);
}

TEST(Vm, InitSetsGuestSpAboveAllocatedPage) {
  resetCaptures();
  Vm vm;
  vm.init(0x40000000ULL, 0x200000ULL, 1, 0x40000000ULL);

  EXPECT_EQ(gVcpuSetGuestSpCap, kAllocPagesSentinel + PAGE_SIZE);
}

TEST(Vm, RunEnablesGuestMmuWithCorrectVmid) {
  resetCaptures();
  Vm vm;
  vm.init(0x40000000ULL, 0x200000ULL, 2, 0x40000000ULL);
  vm.run();

  EXPECT_EQ(gGuestMmuEnableVmid, 2);
}
