// @file vcpu.h
// @brief Per-guest virtual CPU context for EL2 hypervisor scheduling.
// @ingroup vcpu
//
// Defines the Vcpu class, which carries all CPU state needed to suspend a
// guest running at EL1 and resume it later. vcpu.S reaches into it through
// offsets the compiler computes from this layout; see vcpuOffsets.cpp.

#ifndef __VCPU_H__
#define __VCPU_H__

#include <array>
#include <cstddef>
#include <cstdint>

// @brief A saved guest general-purpose register.
// @ingroup vcpu
enum class Gpr : uint8_t {
    X0, X1, X2, X3, X4, X5, X6, X7, X8, X9,
    X10, X11, X12, X13, X14, X15, X16, X17, X18, X19,
    X20, X21, X22, X23, X24, X25, X26, X27, X28, X29,
    LR,
    SP_EL0,
};

// @brief A saved EL2 exception return register.
// @ingroup vcpu
enum class El2Reg : uint8_t {
    ELR_EL2,
    SPSR_EL2,
    COUNT,
};

// @brief A saved EL1 system register.
// @ingroup vcpu
enum class El1Reg : uint8_t {
    SCTLR_EL1,
    TTBR0_EL1,
    TTBR1_EL1,
    TCR_EL1,
    MAIR_EL1,
    AMAIR_EL1,
    VBAR_EL1,
    ELR_EL1,
    SPSR_EL1,
    SP_EL1,
    ESR_EL1,
    FAR_EL1,
    AFSR0_EL1,
    AFSR1_EL1,
    CONTEXTIDR_EL1,
    TPIDR_EL1,
    TPIDR_EL0,
    TPIDRRO_EL0,
    CNTKCTL_EL1,
    CPACR_EL1,
    PAR_EL1,
    CSSELR_EL1,
    COUNT,
};

// @brief Per-guest virtual CPU context.
// @ingroup vcpu
//
// vcpu.S saves and restores the guest's registers straight into this object,
// so the data must stay standard layout: every member private, none virtual.
class alignas(128) Vcpu {
private:
    // Hypervisor state vcpu_enter parks here while the guest runs.
    struct HvContext {
        uint64_t sp;
        uint64_t lr;
        std::array<uint64_t, 11> calleeSaved; // x19..x29
        uint64_t exitEsr;                     // stashed by the exit path before the guest save
    };

    alignas(16) std::array<uint64_t, 31> m_gpr {}; // x0..x30
    uint64_t m_spEl0 {};
    alignas(16) std::array<uint64_t, static_cast<size_t>(El2Reg::COUNT)> m_el2 {};
    alignas(16) std::array<uint64_t, static_cast<size_t>(El1Reg::COUNT)> m_el1 {};
    alignas(16) HvContext m_hvCtx {};
    uint32_t m_id {};

    [[nodiscard]] uint64_t& el1(El1Reg reg) { return m_el1[static_cast<size_t>(reg)]; }
    [[nodiscard]] uint64_t& el2(El2Reg reg) { return m_el2[static_cast<size_t>(reg)]; }
    [[nodiscard]] uint64_t el2(El2Reg reg) const { return m_el2[static_cast<size_t>(reg)]; }

    // vcpuOffsets.cpp reads the private layout to generate vcpu.S's offsets.
    friend struct VcpuLayout;

public:
    // @brief Build this vCPU for first entry into EL1.
    //
    // Starts from zeroed state, then seeds:
    //   - elr_el2   <- entrypoint
    //   - spsr_el2  <- EL1h with all DAIF bits masked
    //   - sctlr_el1 <- hardware reset value with M/C/I/A/SA cleared
    //
    // @param entrypoint Guest physical address to resume at on first eret.
    explicit Vcpu(uint64_t entrypoint);

    // @brief Save EL1 system registers from hardware into this context.
    // @note Call site must have DAIF masked.
    // @return Nothing.
    void SaveEl1SysRegs();

    // @brief Restore EL1 system registers from this context into hardware.
    // @note SCTLR_EL1 is restored last, after TTBR/TCR/MAIR, with an
    //       intervening isb. Call site must have DAIF masked.
    // @return Nothing.
    void RestoreEl1SysRegs();

    // @return The saved guest PC (ELR_EL2).
    [[nodiscard]] uint64_t GetElr() const noexcept;

    // @brief Overwrite the saved guest PC (ELR_EL2).
    // @return Nothing.
    void SetPc(uint64_t pc);

    // @brief Advance ELR_EL2 by 4 bytes (skip faulting instruction).
    // @return Nothing.
    void SkipInstruction();

    // @brief Set the guest SP_EL1 (stack pointer seen by the guest at EL1).
    // @return Nothing.
    void SetGuestSp(uint64_t sp);

    // @return The saved value of @p reg.
    [[nodiscard]] uint64_t GetGpReg(Gpr reg) const noexcept;

    // @brief Overwrite the saved value of @p reg.
    // @return Nothing.
    void SetGpReg(Gpr reg, uint64_t val);

    // @brief The saved x0..x30, for trap handlers that work on the whole set.
    // @return Reference to the saved registers.
    [[nodiscard]] std::array<uint64_t, 31>& GetGprs() noexcept { return m_gpr; }

    // @return Opaque vCPU identifier assigned by the scheduler.
    [[nodiscard]] uint32_t GetId() const noexcept { return m_id; }

    // @brief Set the vCPU identifier (scheduler only).
    // @return Nothing.
    void SetId(uint32_t id) { m_id = id; }

    // @return The Vcpu pointer parked in TPIDR_EL2 on this pCPU.
    [[nodiscard]] static Vcpu* GetCurrentVcpu();

    // @brief Stub scheduler entry. Replaced by real scheduler later.
    // @return Nothing.
    static void ScheduleNext();
};

// @brief Resume the guest state held in @p ctx.
extern "C" void vcpu_enter(Vcpu* ctx);

#endif // !__VCPU_H__
