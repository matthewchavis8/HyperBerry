#ifndef __SMC_H__
#define __SMC_H__

#include "core/vcpu/vcpu.h"

namespace Smc {
void Handle(GuestRegisters& registers);
}

#endif
