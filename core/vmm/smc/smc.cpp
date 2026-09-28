#include "smc.h"
#include "smccc.h"

void Smc::Handle(GuestRegisters& registers) {
    registers.x[0] = SMCCC::ToRegister(SMCCC::NOT_SUPPORTED);
    registers.pc += 4;
}
