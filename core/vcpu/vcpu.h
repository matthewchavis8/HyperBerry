/**
 * @file vcpu.h
 * @brief Per-guest virtual CPU context for EL2 hypervisor scheduling.
 * @ingroup vcpu
 *
 * Defines the Vcpu class, which carries all CPU state needed to
 * suspend a guest running at EL1 and resume it later. Layout matches
 * the offset constants below so that vcpu.S can index into Vcpu
 * instances without C++ knowledge.
 */
#ifndef __VCPU_H__
#define __VCPU_H__

// General-purpose register offsets (from Vcpu::m_gpr base)
#define VCPU_GPREG_X0     0x000
#define VCPU_GPREG_X1     0x008
#define VCPU_GPREG_X2     0x010
#define VCPU_GPREG_X3     0x018
#define VCPU_GPREG_X4     0x020
#define VCPU_GPREG_X5     0x028
#define VCPU_GPREG_X6     0x030
#define VCPU_GPREG_X7     0x038
#define VCPU_GPREG_X8     0x040
#define VCPU_GPREG_X9     0x048
#define VCPU_GPREG_X10    0x050
#define VCPU_GPREG_X11    0x058
#define VCPU_GPREG_X12    0x060
#define VCPU_GPREG_X13    0x068
#define VCPU_GPREG_X14    0x070
#define VCPU_GPREG_X15    0x078
#define VCPU_GPREG_X16    0x080
#define VCPU_GPREG_X17    0x088
#define VCPU_GPREG_X18    0x090
#define VCPU_GPREG_X19    0x098
#define VCPU_GPREG_X20    0x0A0
#define VCPU_GPREG_X21    0x0A8
#define VCPU_GPREG_X22    0x0B0
#define VCPU_GPREG_X23    0x0B8
#define VCPU_GPREG_X24    0x0C0
#define VCPU_GPREG_X25    0x0C8
#define VCPU_GPREG_X26    0x0D0
#define VCPU_GPREG_X27    0x0D8
#define VCPU_GPREG_X28    0x0E0
#define VCPU_GPREG_X29    0x0E8
#define VCPU_GPREG_LR     0x0F0
#define VCPU_GPREG_SP_EL0 0x0F8
#define VCPU_GPREGS_SIZE  0x100

// EL2 exception-return state offsets
#define VCPU_ELR_EL2       0x000
#define VCPU_SPSR_EL2      0x008
#define VCPU_EL2STATE_SIZE 0x010

// EL1 system register offsets
#define VCPU_SCTLR_EL1       0x000
#define VCPU_TTBR0_EL1       0x008
#define VCPU_TTBR1_EL1       0x010
#define VCPU_TCR_EL1         0x018
#define VCPU_MAIR_EL1        0x020
#define VCPU_AMAIR_EL1       0x028
#define VCPU_VBAR_EL1        0x030
#define VCPU_ELR_EL1         0x038
#define VCPU_SPSR_EL1        0x040
#define VCPU_SP_EL1          0x048
#define VCPU_ESR_EL1         0x050
#define VCPU_FAR_EL1         0x058
#define VCPU_AFSR0_EL1       0x060
#define VCPU_AFSR1_EL1       0x068
#define VCPU_CONTEXTIDR_EL1  0x070
#define VCPU_TPIDR_EL1       0x078
#define VCPU_TPIDR_EL0       0x080
#define VCPU_TPIDRRO_EL0     0x088
#define VCPU_CNTKCTL_EL1     0x090
#define VCPU_CPACR_EL1       0x098
#define VCPU_PAR_EL1         0x0A0
#define VCPU_CSSELR_EL1      0x0A8
#define VCPU_EL1SYSREGS_SIZE 0x0B0

// Top-level Vcpu layout offsets
#define VCPU_GPREGS_OFFSET    0x000
#define VCPU_EL2STATE_OFFSET  (VCPU_GPREGS_OFFSET + VCPU_GPREGS_SIZE)
#define VCPU_EL1REGS_OFFSET   (VCPU_EL2STATE_OFFSET + VCPU_EL2STATE_SIZE)
#define VCPU_SIZEOF           (VCPU_EL1REGS_OFFSET + VCPU_EL1SYSREGS_SIZE)

// Hypervisor context offsets
#define VCPU_HVCTX_OFFSET (VCPU_EL1REGS_OFFSET + VCPU_EL1SYSREGS_SIZE)
#define VCPU_HVCTX_SP     0x000
#define VCPU_HVCTX_LR     0x008
#define VCPU_HVCTX_X19    0x010
#define VCPU_HVCTX_X20    0x018
#define VCPU_HVCTX_X21    0x020
#define VCPU_HVCTX_X22    0x028
#define VCPU_HVCTX_X23    0x030
#define VCPU_HVCTX_X24    0x038
#define VCPU_HVCTX_X25    0x040
#define VCPU_HVCTX_X26    0x048
#define VCPU_HVCTX_X27    0x050
#define VCPU_HVCTX_X28    0x058
#define VCPU_HVCTX_X29      0x060
#define VCPU_HVCTX_EXIT_ESR 0x068

#define VCPU_HVCTX_SIZE 0x070
#ifndef __ASSEMBLER__

#include <stdint.h>
#include "lib/array/array.h"

static constexpr size_t regIdx(size_t off) {
  return off / sizeof(uint64_t);
}

/**
 * @brief EL2 exception-return state: elr_el2, spsr_el2.
 * @ingroup vcpu
 */
struct El2State {
  hv::array<uint64_t, VCPU_EL2STATE_SIZE / sizeof(uint64_t)> regs;
} __attribute__((aligned(16)));

/**
 * @brief EL1 system register context (SCTLR_EL1, TTBRn_EL1, etc).
 * @ingroup vcpu
 */
struct El1SysRegs {
  hv::array<uint64_t, VCPU_EL1SYSREGS_SIZE / sizeof(uint64_t)> regs;
} __attribute__((aligned(16)));

struct HvContext {
  uint64_t sp;
  uint64_t lr;
  hv::array<uint64_t, 11> callSavedReg;
  uint64_t exitEsr;  // stashed by vcpu_exit_sync/serror before guest state save
} __attribute__((aligned(16)));

/**
 * @brief Per-guest virtual CPU context.
 * @ingroup vcpu
 *
 * Layout-critical: the first three sub-structs are indexed from vcpu.S
 * via the VCPU_*_OFFSET constants above. Fields below the
 * "asm contract line" comment are free to reorder.
 *
 * All data is private; the class exposes methods for every operation
 * a caller might need. This preserves standard-layout (all non-static
 * data members share the same access specifier) while hiding the raw
 * register arrays from normal C++ callers.
 */
class Vcpu {
private:
  friend struct VcpuLayoutAccess;

  hv::array<uint64_t, 31> m_gpr;
  uint64_t   m_spEl0;
  El2State   m_el2State;
  El1SysRegs m_el1SysRegs;
  HvContext  m_hvCtx;
  uint32_t   m_vcpuId;

public:
  /**
   * @brief Initialise this vCPU for first entry into EL1.
   *
   * Zeroes all state, then seeds:
   *   - elr_el2   ← entrypoint
   *   - spsr_el2  ← EL1h with all DAIF bits masked
   *   - sctlr_el1 ← hardware reset value with M/C/I/A/SA cleared
   *
   * @param entrypoint Guest physical address to resume at on first eret.
   */
  void init(uint64_t entrypoint);
  
  /**
   * @brief Save EL1 system registers from hardware into this context.
   * @note Call site must have DAIF masked. Meaningful only on AArch64.
   */
  void saveEl1SysRegs();

  /**
   * @brief Restore EL1 system registers from this context into hardware.
   * @note SCTLR_EL1 is restored last, after TTBR/TCR/MAIR, with an
   *       intervening isb. Call site must have DAIF masked.
   */
  void restoreEl1SysRegs();

  /** @brief Return the saved guest PC (ELR_EL2). */
  uint64_t getElr() const;

  /** @brief Overwrite the saved guest PC (ELR_EL2). */
  void setPc(uint64_t pc);

  /** @brief Advance ELR_EL2 by 4 bytes (skip faulting instruction). */
  void skipInstruction();

  /** @brief Set the guest SP_EL1 (stack pointer seen by the guest at EL1). */
  void setGuestSp(uint64_t sp);

  /**
   * @brief Read a saved GPR by offset.
   * @param off One of the VCPU_GPREG_* constants.
   */
  uint64_t getGpReg(uint64_t off) const;

  /**
   * @brief Write a saved GPR by offset.
   * @param off One of the VCPU_GPREG_* constants.
   * @param val Value to store.
   */
  void setGpReg(uint64_t off, uint64_t val);

  /** @brief Opaque vCPU identifier assigned by the scheduler. */
  uint32_t getId() const { return m_vcpuId; }

  /** @brief Set the vCPU identifier (scheduler-only). */
  void setId(uint32_t vcpuId) { m_vcpuId = vcpuId; }

  /**
   * @brief Return the Vcpu pointer parked in TPIDR_EL2 on this pCPU.
   * @note Returns nullptr on hosted (non-AArch64) builds.
   */
  static Vcpu* getCurrentVcpu();

  /**
   * @brief Stub scheduler entry. Replaced by real scheduler later.
   */
  static void scheduleNext();
} __attribute__((aligned(128)));

struct VcpuLayoutAccess {
  static constexpr uint64_t gprOffset() { return __builtin_offsetof(Vcpu, m_gpr); }

  static constexpr uint64_t spEl0Offset() { return __builtin_offsetof(Vcpu, m_spEl0); }
  
  static constexpr uint64_t hvCtxOffset() { return __builtin_offsetof(Vcpu, m_hvCtx); }

  static constexpr uint64_t el2StateOffset() { return __builtin_offsetof(Vcpu, m_el2State); }

  static constexpr uint64_t el1SysRegsOffset() { return __builtin_offsetof(Vcpu, m_el1SysRegs); }
};

// Fail Loudly
static_assert(__is_standard_layout(Vcpu),
  "Vcpu must be standard-layout so .S can rely on member offsets");
static_assert(sizeof(El2State) == VCPU_EL2STATE_SIZE,
  "El2State size drifted from the asm-visible EL2 layout");
static_assert(sizeof(El1SysRegs) == VCPU_EL1SYSREGS_SIZE,
  "El1SysRegs size drifted from the asm-visible EL1 sysreg layout");
static_assert(VcpuLayoutAccess::gprOffset() == VCPU_GPREGS_OFFSET,
  "m_gpr offset drifted from VCPU_GPREGS_OFFSET");
static_assert(VcpuLayoutAccess::spEl0Offset() == VCPU_GPREG_SP_EL0,
  "m_spEl0 offset drifted from VCPU_GPREG_SP_EL0");
static_assert(VcpuLayoutAccess::el2StateOffset() == VCPU_EL2STATE_OFFSET,
  "m_el2State offset drifted from VCPU_EL2STATE_OFFSET");
static_assert(VcpuLayoutAccess::el1SysRegsOffset() == VCPU_EL1REGS_OFFSET,
  "m_el1SysRegs offset drifted from VCPU_EL1REGS_OFFSET");
static_assert(sizeof(Vcpu) >= VCPU_SIZEOF,
  "Vcpu smaller than asm-expected context size");

static_assert(sizeof(HvContext) == VCPU_HVCTX_SIZE,
  "HvContext size drifted from asm-visible layout");
static_assert(VcpuLayoutAccess::hvCtxOffset() == VCPU_HVCTX_OFFSET,
  "m_hvCtx offset drifted from VCPU_HVCTX_OFFSET");

// @brief fn used to resume guest state
extern "C" void vcpu_enter(Vcpu* ctx);

#endif /* __ASSEMBLER__ */
#endif /* !__VCPU_H__ */
