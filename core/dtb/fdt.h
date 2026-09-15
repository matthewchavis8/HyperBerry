// @file core/dtb/fdt.h
// @brief Flattened Device Tree on-wire format and shared decoding helpers.
// @ingroup core
//
// Both the read-only boot parser and the guest DTB patcher walk the same
// structure block, so the token values, header layout and byte-order helpers
// live here rather than being duplicated per walker.

#ifndef __FDT_H__
#define __FDT_H__

#include <stdint.h>

// @brief Flattened Device Tree token values used in the structure block.
enum class FDT : uint32_t {
    MAGIC = 0xD00DFEED,
    BEGIN_NODE = 1,
    END_NODE = 2,
    PROP = 3,
    NOP = 4,
    END = 9,
};

// @brief On-wire DTB header layout.
struct FdtHeader {
    uint32_t magic;
    uint32_t totalSize;
    uint32_t structOff;
    uint32_t stringsOff;
    uint32_t memRsvMapOff;
    uint32_t version;
    uint32_t lastCompVersion;
    uint32_t bootCpuId;
    uint32_t sizeStrings;
    uint32_t sizeStructs;
};

// @brief DTB property record header.
struct FdtProp {
    uint32_t dataLen;
    uint32_t nameOff;
};

// @brief Convert a 32-bit big-endian DTB field to host endianness.
inline uint32_t Be32(uint32_t byte) {
    return __builtin_bswap32(byte);
}

// @brief Convert a 64-bit big-endian DTB field to host endianness.
inline uint64_t Be64(uint64_t byte) {
    return __builtin_bswap64(byte);
}

// @brief Compare two null-terminated strings for equality.
inline bool StrEq(const char* str1, const char* str2) {
    while (*str1 && *str2) {
        if (*str1 != *str2) return false;
        str1++;
        str2++;
    }

    return *str1 == *str2;
}

// @brief Test whether @p str begins with @p prefix.
inline bool StrStartsWith(const char* str, const char* prefix) {
    while (*prefix) {
        if (*str != *prefix) return false;
        str++;
        prefix++;
    }

    return true;
}

// @brief Advance past @p bytes and round up to the next 32-bit boundary.
// @return The aligned address; callers cast to the constness they need.
//
// The structure block only guarantees 4-byte alignment, so every token and
// property payload has to be re-aligned before the next read.
inline uintptr_t FdtAlign(const void* ptr, uint32_t bytes) {
    uintptr_t addr { reinterpret_cast<uintptr_t>(ptr) + bytes };

    return (addr + 3) & ~static_cast<uintptr_t>(3);
}

#endif // __FDT_H__
