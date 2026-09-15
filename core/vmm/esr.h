// @file esr.h
// @brief Saved exception frame and ESR_EL2 field decode.
// @ingroup vmm

#ifndef __ESR_H__
#define __ESR_H__

#include <stdint.h>
#include "lib/array/array.h"

// ESR_EL2.EC, the exception class in bits [31:26]
enum class EsrEc : uint8_t {
    UNKNOWN = 0x00,
    WFX_TRAP = 0x01,
    CP15_MCR_MRC = 0x03,
    CP15_MCRR_MRRC = 0x04,
    CP14_MCR_MRC = 0x05,
    CP14_LDC_STC = 0x06,
    FP_ACCESS = 0x07,
    PAC_TRAP = 0x09,
    CP14_MRRC = 0x0C,
    BRANCH_TARGET = 0x0D,
    ILLEGAL_STATE = 0x0E,
    SVC_AARCH32 = 0x11,
    HVC_AARCH32 = 0x12,
    SMC_AARCH32 = 0x13,
    SVC_AARCH64 = 0x15,
    HVC_AARCH64 = 0x16,
    SMC_AARCH64 = 0x17,
    MSR_MRS_TRAP = 0x18,
    SVE_ACCESS = 0x19,
    FPAC_FAILURE = 0x1C,
    INSTR_ABORT_LOWER = 0x20,
    INSTR_ABORT_SAME = 0x21,
    PC_ALIGN_FAULT = 0x22,
    DATA_ABORT_LOWER = 0x24,
    DATA_ABORT_SAME = 0x25,
    SP_ALIGN_FAULT = 0x26,
    FP_EXC_AARCH32 = 0x28,
    FP_EXC_AARCH64 = 0x2C,
    SERROR = 0x2F,
    BREAKPOINT_LOWER = 0x30,
    BREAKPOINT_SAME = 0x31,
    SOFT_STEP_LOWER = 0x32,
    SOFT_STEP_SAME = 0x33,
    WATCHPOINT_LOWER = 0x34,
    WATCHPOINT_SAME = 0x35,
    BKPT_AARCH32 = 0x38,
    VECTOR_CATCH = 0x3A,
    BRK_AARCH64 = 0x3C,
};

// x0 to x30 as saved by the EL2 entry glue
using ExceptionContext = hv::array<uint64_t, 31>;

// @brief Extract the exception class from a raw ESR_EL2 value.
// @param esr Raw ESR_EL2.
// @return The EC field, bits [31:26].
constexpr EsrEc getEsrEc(uint64_t esr) {
    return static_cast<EsrEc>((esr >> 26) & 0x3F);
}

// @brief Extract the instruction specific syndrome from a raw ESR_EL2 value.
// @param esr Raw ESR_EL2.
// @return The ISS field, bits [24:0].
constexpr uint32_t getEsrIss(uint64_t esr) {
    return static_cast<uint32_t>(esr & 0x1FFFFFF);
}

#endif // !__ESR_H__
