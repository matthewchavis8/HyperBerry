/**
 * @file vm.cpp
 * @brief Per-guest VM container implementation.
 * @ingroup vm
 */

#include "core/mm/pmm/pmm.h"
#include "drivers/uart/uart.h"
#include "vm.h"

void Vm::init(const char* name, uint64_t ipaBase, uint64_t sizeBytes, uint8_t vmid,
              uint64_t guestEntry) {
  m_name = name;
  m_vmid = vmid;

  Uart::println("[VM] Bringing up Guest MMU");
  m_guestMmu.init(ipaBase, sizeBytes);
  Uart::println("[VM] Bringing up Guest MMU");

  Uart::println("[VM] Bringing up Vcpu");
  m_vcpu.init(guestEntry);

  uint64_t guestStackBase = pmm::allocPages(0);
  m_vcpu.setGuestSp(guestStackBase + PAGE_SIZE);
}

void Vm::run() {
  Uart::println("[VM] Enabling Guest MMU");
  m_guestMmu.enable(m_vmid);
  Uart::println("[VM] Successfully enabled Guest MMU");

  Uart::println("[VM] Guest Kernel Running");
  vcpu_enter(&m_vcpu);
}

[[nodiscard]] const char* Vm::getName() const { return m_name; }
