// @file vcpu.h
// @brief Saved guest state and one guest execution interval.

#ifndef __VCPU_H__
#define __VCPU_H__

#include <array>
#include <cstdint>

struct GuestRegisters {
    std::array<uint64_t, 31> x {};
    uint64_t pc {};
    uint64_t pstate { 0x3C5 }; // EL1h with DAIF masked
    uint64_t spEl0 {};
    uint64_t spEl1 {};
};

enum class ExitReason : uint64_t {
    SYNC,
    IRQ,
    FIQ,
    SERROR,
};

struct VcpuExit {
    ExitReason reason {};
    uint64_t syndrome {};
    uint64_t far {};
    uint64_t hpfar {};
};

class Vcpu {
private:
    struct SystemRegisters {
        uint64_t sctlr {};
        uint64_t ttbr0 {};
        uint64_t ttbr1 {};
        uint64_t tcr {};
        uint64_t mair {};
        uint64_t amair {};
        uint64_t vbar {};
        uint64_t elr {};
        uint64_t spsr {};
        uint64_t esr {};
        uint64_t far {};
        uint64_t afsr0 {};
        uint64_t afsr1 {};
        uint64_t contextidr {};
        uint64_t tpidr {};
        uint64_t tpidrEl0 {};
        uint64_t tpidrroEl0 {};
        uint64_t cntkctl {};
        uint64_t cpacr {};
        uint64_t par {};
        uint64_t csselr {};
    };

    GuestRegisters m_registers;
    SystemRegisters m_system;

    friend struct VcpuLayout;

public:
    // @brief Prepare an EL1 guest with its MMU and caches disabled.
    // @return A stopped virtual CPU.
    explicit Vcpu(uint64_t entry);

    // @brief Run until the next guest exception, then return with state saved.
    // @return The exception kind, syndrome and abort addresses.
    [[nodiscard]] VcpuExit Run();

    // @return Saved guest registers, writable while the guest is stopped.
    [[nodiscard]] GuestRegisters& GetRegisters() noexcept;

    // @return Saved guest registers.
    [[nodiscard]] const GuestRegisters& GetRegisters() const noexcept;

    Vcpu(const Vcpu&) = delete;
    Vcpu& operator=(const Vcpu&) = delete;
    Vcpu(Vcpu&&) = delete;
    Vcpu& operator=(Vcpu&&) = delete;
    ~Vcpu() = default;
};

#endif // __VCPU_H__
