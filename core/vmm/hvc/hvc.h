#ifndef __HVC_H__
#define __HVC_H__

#include <cstdint>
#include "core/vcpu/vcpu.h"

namespace Hvc {

enum class Action : uint8_t { RESUME, SHUTDOWN, RESET };

[[nodiscard]] Action Handle(GuestRegisters& registers, uint32_t immediate);

}

#endif
