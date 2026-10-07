#include "vmm.h"
#include "core/vmm/esr/esr.h"
#include "core/vmm/hvc/hvc.h"
#include "core/vmm/smc/smc.h"
#include "lib/log/log.h"

Vmm::Vmm(const VmConfig& config, const MmioMap& devices) : m_vm { config, devices } {}

std::expected<VmState, RunError> Vmm::Run() {
    if (m_vm.GetState() != VmState::READY) {
        return std::unexpected(RunError::INVALID_STATE);
    }

    m_vm.Start();

    for (;;) {
        const VcpuExit exit { m_vm.Enter() };
        auto& registers { m_vm.GetRegisters() };

        if (exit.reason == ExitReason::IRQ || exit.reason == ExitReason::FIQ) {
            continue;
        }

        if (exit.reason == ExitReason::SERROR) {
            m_vm.Stop(VmState::FAULTED);
            return m_vm.GetState();
        }

        const EsrEc exceptionClass { GetEsrEc(exit.syndrome) };
        switch (exceptionClass) {
            case EsrEc::HVC_AARCH64: {
                const Hvc::Action action { Hvc::Handle(registers, GetEsrIss(exit.syndrome)) };
                if (action == Hvc::Action::SHUTDOWN) {
                    m_vm.Stop(VmState::SHUTDOWN);
                }
                if (action == Hvc::Action::RESET) {
                    m_vm.Stop(VmState::RESET_REQUESTED);
                }
                break;
            }
            case EsrEc::SMC_AARCH64:
                Smc::Handle(registers);
                break;
            default:
                Log::Println(
                        "[Guest] Unhandled exception EC={:x} ISS={:x} ESR={:x} FAR={:x} HPFAR={:x}",
                        GetEsrEc(exit.syndrome),
                        GetEsrIss(exit.syndrome),
                        exit.syndrome,
                        exit.far,
                        exit.hpfar);
                m_vm.Stop(VmState::FAULTED);
                break;
        }
        if (m_vm.GetState() != VmState::RUNNING) {
            return m_vm.GetState();
        }
    }
}

VmState Vmm::GetState() const noexcept {
    return m_vm.GetState();
}

const VcpuExit& Vmm::GetLastExit() const noexcept {
    return m_vm.GetLastExit();
}
