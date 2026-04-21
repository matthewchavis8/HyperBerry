/**
 * @file vm.cpp
 * @brief Per-guest VM container implementation.
 * @ingroup vm
 */

#include "core/mm/pmm/pmm.h"
#include "drivers/uart/uart.h"
#include "vm.h"

void Vm::init(uint64_t ipaBase, uint64_t sizeBytes, uint8_t vmid,
              uint64_t guestEntry) {
  m_vmid = vmid;

  Uart::println("[Vm] Building stage-2 mappings");
  m_guestMmu.init(ipaBase, sizeBytes);

  Uart::println("[Vm] Initialising vCPU");
  m_vcpu.init(guestEntry);

  uint64_t guestStackBase = pmm::allocPages(0);
  m_vcpu.setGuestSp(guestStackBase + PAGE_SIZE);
}

void Vm::run() {
  Uart::println("[Vm] Enabling stage-2 translation");
  m_guestMmu.enable(m_vmid);

  Uart::println("[Vm] Entering guest");
  vcpu_enter(&m_vcpu);
}
