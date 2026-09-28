#ifndef __VMM_H__
#define __VMM_H__

#include <expected>
#include "core/vm/vm.h"

enum class RunError : uint8_t { INVALID_STATE };

class Vmm {
private:
    Vm m_vm;

public:
    Vmm(const VmConfig& config, const MmioMap& devices);

    [[nodiscard]] std::expected<VmState, RunError> Run();
    [[nodiscard]] VmState GetState() const noexcept;
    [[nodiscard]] const VcpuExit& GetLastExit() const noexcept;
};

#endif
