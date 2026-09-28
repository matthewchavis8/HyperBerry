#ifndef __VCPU_TEST_BACKEND_H__
#define __VCPU_TEST_BACKEND_H__

#include "core/vcpu/vcpu.h"
#include <functional>

namespace vcpuTest {
extern std::function<VcpuExit(Vcpu&)> run;
}

#endif // __VCPU_TEST_BACKEND_H__
