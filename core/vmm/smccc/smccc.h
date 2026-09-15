// @file smccc.h
// @brief Arm SMC Calling Convention function ID fields, owners and return codes.
// @ingroup smccc

#ifndef __SMCCC_H__
#define __SMCCC_H__

#include <stdint.h>

namespace SMCCC {

inline constexpr uint32_t OEN_SHIFT { 24 }; // owning entity number, bits [29:24]
inline constexpr uint32_t OEN_MASK { 0x3F };

inline constexpr uint32_t TYPE_SHIFT { 31 }; // call type, bit [31]
inline constexpr uint32_t TYPE_MASK { 0x1 };

inline constexpr uint32_t TYPE_YIELDING { 0 };
inline constexpr uint32_t TYPE_FAST { 1 };

inline constexpr uint32_t OWNER_ARCH { 0 };
inline constexpr uint32_t OWNER_CPU { 1 };
inline constexpr uint32_t OWNER_SIP { 2 }; // silicon partner
inline constexpr uint32_t OWNER_OEM { 3 };
inline constexpr uint32_t OWNER_STANDARD { 4 }; // PSCI
inline constexpr uint32_t OWNER_STANDARD_HYP { 5 };
inline constexpr uint32_t OWNER_VENDOR_HYP { 6 };
inline constexpr uint32_t OWNER_TRUSTED_APP { 48 };
inline constexpr uint32_t OWNER_TRUSTED_OS { 50 };

inline constexpr int32_t SUCCESS { 0 };
inline constexpr int32_t NOT_SUPPORTED { -1 };
inline constexpr int32_t NOT_REQUIRED { -2 };
inline constexpr int32_t INVALID_PARAMETER { -3 };

// @brief Widen a return code to the value a caller reads back in x0.
// @param code One of the SMCCC return codes.
// @return The code sign extended to 64 bits.
constexpr uint64_t toRegister(int32_t code) {
    return static_cast<uint64_t>(static_cast<int64_t>(code));
}

// @brief Extract the owning entity number from an SMCCC function ID.
// @param funcId Raw function ID from x0.
// @return The OEN field.
constexpr uint32_t getOwner(uint64_t funcId) {
    return (funcId >> OEN_SHIFT) & OEN_MASK;
}

// @brief Check whether an SMCCC function ID is a fast call.
// @param funcId Raw function ID from x0.
// @return true when the call type bit is TYPE_FAST.
constexpr bool isFastCall(uint64_t funcId) {
    return ((funcId >> TYPE_SHIFT) & TYPE_MASK) == TYPE_FAST;
}

} // namespace SMCCC

#endif // !__SMCCC_H__
