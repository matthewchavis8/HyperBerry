// @file vcpuOffsets.cpp
// @brief Computes the Vcpu offsets vcpu.S indexes with.
// @ingroup vcpu
//
// Never linked. The build compiles this file to assembly text, and
// scripts/asmoffsets/asmoffsets.py turns each marker into a #define in the
// generated vcpuOffsets.h. The assembly therefore always reads the layout the
// compiler actually chose, instead of a hand kept copy of it.

#include "vcpu.h"

#include <cstddef>
#include <type_traits>

// Stringifying the symbol name is the one thing a function cannot do.
#define OFFSET(sym, val) asm volatile("\n.ascii \"->" #sym " %c0\"" ::"i"(val))

struct VcpuLayout {
    static constexpr size_t REG_BYTES { sizeof(uint64_t) };

    static constexpr size_t gpr(Gpr reg) { return static_cast<size_t>(reg) * REG_BYTES; }
    static constexpr size_t el2(El2Reg reg) { return static_cast<size_t>(reg) * REG_BYTES; }

    static constexpr size_t hvCallee(size_t reg) {
        return offsetof(Vcpu::HvContext, calleeSaved) + (reg - 19) * REG_BYTES;
    }

    static void emit() {
        static_assert(std::is_standard_layout_v<Vcpu>, "offsetof needs a standard layout Vcpu");

        OFFSET(VCPU_GPREGS_OFFSET, offsetof(Vcpu, m_gpr));
        OFFSET(VCPU_GPREG_X0, gpr(Gpr::X0));
        OFFSET(VCPU_GPREG_X1, gpr(Gpr::X1));
        OFFSET(VCPU_GPREG_X2, gpr(Gpr::X2));
        OFFSET(VCPU_GPREG_X3, gpr(Gpr::X3));
        OFFSET(VCPU_GPREG_X4, gpr(Gpr::X4));
        OFFSET(VCPU_GPREG_X5, gpr(Gpr::X5));
        OFFSET(VCPU_GPREG_X6, gpr(Gpr::X6));
        OFFSET(VCPU_GPREG_X7, gpr(Gpr::X7));
        OFFSET(VCPU_GPREG_X8, gpr(Gpr::X8));
        OFFSET(VCPU_GPREG_X9, gpr(Gpr::X9));
        OFFSET(VCPU_GPREG_X10, gpr(Gpr::X10));
        OFFSET(VCPU_GPREG_X11, gpr(Gpr::X11));
        OFFSET(VCPU_GPREG_X12, gpr(Gpr::X12));
        OFFSET(VCPU_GPREG_X13, gpr(Gpr::X13));
        OFFSET(VCPU_GPREG_X14, gpr(Gpr::X14));
        OFFSET(VCPU_GPREG_X15, gpr(Gpr::X15));
        OFFSET(VCPU_GPREG_X16, gpr(Gpr::X16));
        OFFSET(VCPU_GPREG_X17, gpr(Gpr::X17));
        OFFSET(VCPU_GPREG_X18, gpr(Gpr::X18));
        OFFSET(VCPU_GPREG_X19, gpr(Gpr::X19));
        OFFSET(VCPU_GPREG_X20, gpr(Gpr::X20));
        OFFSET(VCPU_GPREG_X21, gpr(Gpr::X21));
        OFFSET(VCPU_GPREG_X22, gpr(Gpr::X22));
        OFFSET(VCPU_GPREG_X23, gpr(Gpr::X23));
        OFFSET(VCPU_GPREG_X24, gpr(Gpr::X24));
        OFFSET(VCPU_GPREG_X25, gpr(Gpr::X25));
        OFFSET(VCPU_GPREG_X26, gpr(Gpr::X26));
        OFFSET(VCPU_GPREG_X27, gpr(Gpr::X27));
        OFFSET(VCPU_GPREG_X28, gpr(Gpr::X28));
        OFFSET(VCPU_GPREG_X29, gpr(Gpr::X29));
        OFFSET(VCPU_GPREG_LR, gpr(Gpr::LR));
        OFFSET(VCPU_GPREG_SP_EL0, offsetof(Vcpu, m_spEl0) - offsetof(Vcpu, m_gpr));

        OFFSET(VCPU_EL2STATE_OFFSET, offsetof(Vcpu, m_el2));
        OFFSET(VCPU_ELR_EL2, el2(El2Reg::ELR_EL2));
        OFFSET(VCPU_SPSR_EL2, el2(El2Reg::SPSR_EL2));

        OFFSET(VCPU_HVCTX_OFFSET, offsetof(Vcpu, m_hvCtx));
        OFFSET(VCPU_HVCTX_SP, offsetof(Vcpu::HvContext, sp));
        OFFSET(VCPU_HVCTX_LR, offsetof(Vcpu::HvContext, lr));
        OFFSET(VCPU_HVCTX_X19, hvCallee(19));
        OFFSET(VCPU_HVCTX_X20, hvCallee(20));
        OFFSET(VCPU_HVCTX_X21, hvCallee(21));
        OFFSET(VCPU_HVCTX_X22, hvCallee(22));
        OFFSET(VCPU_HVCTX_X23, hvCallee(23));
        OFFSET(VCPU_HVCTX_X24, hvCallee(24));
        OFFSET(VCPU_HVCTX_X25, hvCallee(25));
        OFFSET(VCPU_HVCTX_X26, hvCallee(26));
        OFFSET(VCPU_HVCTX_X27, hvCallee(27));
        OFFSET(VCPU_HVCTX_X28, hvCallee(28));
        OFFSET(VCPU_HVCTX_X29, hvCallee(29));
        OFFSET(VCPU_HVCTX_EXIT_ESR, offsetof(Vcpu::HvContext, exitEsr));
    }
};

extern "C" void vcpu_offsets() {
    VcpuLayout::emit();
}
