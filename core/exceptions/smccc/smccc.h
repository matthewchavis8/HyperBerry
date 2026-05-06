/**
 * @file smccc.h
 * @brief ARM Secure Monitor Call Calling Convention (SMCCC) definitions.
 */

#ifndef __SMCCC_H__
#define __SMCCC_H__

#include <stdint.h>

namespace SMCCC {
    // Bitfield shifts and masks for SMCCC function IDs.
    inline constexpr uint32_t OEN_SHIFT       = 24;   // OEN field starts at bit[29:24]
    inline constexpr uint32_t OEN_MASK        = 0x3F; // OEN field is 6 bits wide
    
    inline constexpr uint32_t TYPE_SHIFT      = 31;   // Call type bit at bit[31]
    inline constexpr uint32_t TYPE_MASK       = 0x1;  // Single-bit type mask

    // Call Types & Conventions
    inline constexpr uint32_t TYPE_YIELDING   = 0;    // Standard/yielding call
    inline constexpr uint32_t TYPE_FAST       = 1;    // Fast call

    // Owner Entity Numbers (OEN)
    inline constexpr uint32_t OWNER_ARCH         = 0;  // Architecture service calls
    inline constexpr uint32_t OWNER_CPU          = 1;  // CPU service calls
    inline constexpr uint32_t OWNER_SIP          = 2;  // Silicon Partner service calls
    inline constexpr uint32_t OWNER_OEM          = 3;  // OEM service calls
    inline constexpr uint32_t OWNER_STANDARD     = 4;  // Standard service calls (e.g. PSCI)
    inline constexpr uint32_t OWNER_STANDARD_HYP = 5;  // Standard hypervisor service calls
    inline constexpr uint32_t OWNER_VENDOR_HYP   = 6;  // Vendor hypervisor service calls
    inline constexpr uint32_t OWNER_TRUSTED_APP  = 48; // Trusted Application calls
    inline constexpr uint32_t OWNER_TRUSTED_OS   = 50; // Trusted OS calls

    // Standard Return Codes
    inline constexpr int32_t SUCCESS                 = 0;  // Call completed successfully
    inline constexpr int32_t NOT_SUPPORTED           = -1; // Function ID is not supported
    inline constexpr int32_t NOT_REQUIRED            = -2; // Operation is not required
    inline constexpr int32_t INVALID_PARAMETER       = -3; // One or more arguments are invalid

    // Extracts the Owner Entity Number from the call ID
    constexpr uint32_t getOwner(uint64_t funcId) {
        return (funcId >> OEN_SHIFT) & OEN_MASK;
    }

    // Checks if a call is a Fast call (SMC64/HVC64)
    constexpr bool isFastCall(uint64_t funcId) {
        return ((funcId >> TYPE_SHIFT) & TYPE_MASK) == TYPE_FAST;
    }

} // namespace SMCCC

#endif // __SMCCC_H__
