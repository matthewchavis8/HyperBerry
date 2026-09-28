// @file vm.h
// @brief A guest address space and its single virtual CPU.

#ifndef __VM_H__
#define __VM_H__

#include <cstdint>
#include <string_view>

#include "core/mm/mmu/guestMmu/guestMmu.h"
#include "core/vcpu/vcpu.h"

// @brief Where a guest lives and how it starts.
// @ingroup vm
struct VmConfig {
    std::string_view name; // guest kernel or VM name, for diagnostics
    uint64_t ipaBase;      // guest IPA base
    uint64_t ramHostPa;    // host physical base backing guest RAM
    uint64_t ramSize;      // size of the guest RAM region
    uint8_t vmid;          // nonzero, unique across live VMs
    uint64_t entry;        // guest IPA to resume at on first eret
    uint64_t dtb;          // guest IPA of the Linux device tree blob
};

class Vm {
private:
    std::string_view m_name;
    GuestMmu m_guestMmu;
    Vcpu m_vcpu;
    uint8_t m_vmid;

public:
    // @brief Build the guest mappings and seed the Linux boot registers.
    // @return A guest ready to run.
    Vm(const VmConfig& config, const MmioMap& devices);

    // @brief Activate the guest address space and handle exits as they occur.
    // @return Does not return.
    [[noreturn]] void Run();

    // @return The guest name passed in @ref VmConfig.
    [[nodiscard]] std::string_view GetName() const noexcept;

    // @return The VMID passed in @ref VmConfig.
    [[nodiscard]] uint8_t GetVmId() const noexcept;

    Vm(const Vm&) = delete;
    Vm& operator=(const Vm&) = delete;
    Vm(Vm&&) = delete;
    Vm& operator=(Vm&&) = delete;
    ~Vm() = default;
};

#endif // !__VM_H__
