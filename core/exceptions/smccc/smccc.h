/**
 * @file smccc.h
 * @brief ARM Secure Monitor Call Calling Convention (SMCCC) definitions.
 * @ingroup smccc
 */

#ifndef __SMCCC_H__
#define __SMCCC_H__

#include <stdint.h>

/**
 * @brief SMCCC function-ID fields, owner numbers, return codes, and helpers.
 */
namespace SMCCC {
    /** OEN field start bit in an SMCCC function ID. */
    inline constexpr uint32_t OEN_SHIFT       = 24;   // OEN field starts at bit[29:24]
    /** OEN field mask after shifting. */
    inline constexpr uint32_t OEN_MASK        = 0x3F; // OEN field is 6 bits wide
    
    /** Call-type field start bit in an SMCCC function ID. */
    inline constexpr uint32_t TYPE_SHIFT      = 31;   // Call type bit at bit[31]
    /** Call-type field mask after shifting. */
    inline constexpr uint32_t TYPE_MASK       = 0x1;  // Single-bit type mask

    /** Yielding call type. */
    inline constexpr uint32_t TYPE_YIELDING   = 0;    // Standard/yielding call
    /** Fast call type. */
    inline constexpr uint32_t TYPE_FAST       = 1;    // Fast call

    /** Architecture service owner number. */
    inline constexpr uint32_t OWNER_ARCH         = 0;  // Architecture service calls
    /** CPU service owner number. */
    inline constexpr uint32_t OWNER_CPU          = 1;  // CPU service calls
    /** Silicon Partner service owner number. */
    inline constexpr uint32_t OWNER_SIP          = 2;  // Silicon Partner service calls
    /** OEM service owner number. */
    inline constexpr uint32_t OWNER_OEM          = 3;  // OEM service calls
    /** Standard service owner number, used here for PSCI. */
    inline constexpr uint32_t OWNER_STANDARD     = 4;  // Standard service calls (e.g. PSCI)
    /** Standard hypervisor service owner number. */
    inline constexpr uint32_t OWNER_STANDARD_HYP = 5;  // Standard hypervisor service calls
    /** Vendor hypervisor service owner number. */
    inline constexpr uint32_t OWNER_VENDOR_HYP   = 6;  // Vendor hypervisor service calls
    /** Trusted Application service owner number. */
    inline constexpr uint32_t OWNER_TRUSTED_APP  = 48; // Trusted Application calls
    /** Trusted OS service owner number. */
    inline constexpr uint32_t OWNER_TRUSTED_OS   = 50; // Trusted OS calls

    /** SMCCC success return code. */
    inline constexpr int32_t SUCCESS                 = 0;  // Call completed successfully
    /** SMCCC unsupported-function return code. */
    inline constexpr int32_t NOT_SUPPORTED           = -1; // Function ID is not supported
    /** SMCCC not-required return code. */
    inline constexpr int32_t NOT_REQUIRED            = -2; // Operation is not required
    /** SMCCC invalid-parameter return code. */
    inline constexpr int32_t INVALID_PARAMETER       = -3; // One or more arguments are invalid

    /**
     * @brief Extract the Owner Entity Number from an SMCCC function ID.
     *
     * @param funcId Raw SMCCC function ID from x0.
     * @return Owner Entity Number field.
     */
    constexpr uint32_t getOwner(uint64_t funcId) {
        return (funcId >> OEN_SHIFT) & OEN_MASK;
    }

    /**
     * @brief Check whether an SMCCC function ID uses the fast-call type.
     *
     * @param funcId Raw SMCCC function ID from x0.
     * @return true when the call type is SMCCC::TYPE_FAST.
     */
    constexpr bool isFastCall(uint64_t funcId) {
        return ((funcId >> TYPE_SHIFT) & TYPE_MASK) == TYPE_FAST;
    }

} // namespace SMCCC

#endif // __SMCCC_H__
