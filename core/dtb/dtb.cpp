/**
 * @file dtb.cpp
 * @brief Flattened Device Tree parser used during early boot.
 * @ingroup core
 */

#include "dtb.h"
#include "drivers/uart/uart.h"
#include <stdint.h>

/**
 * @brief Flattened Device Tree token values used in the structure block.
 */
enum class FDT : uint32_t {
  MAGIC      = 0xD00DFEED,
  BEGIN_NODE = 1,
  END_NODE   = 2,
  PROP       = 3,
  NOP        = 4,
  END        = 9,
};

/**
 * @brief On-wire DTB header layout.
 */
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

/**
 * @brief DTB property record header.
 */
struct FdtProp {
  uint32_t dataLen;
  uint32_t nameOff;
};

/**
 * @brief Convert a 32-bit big-endian DTB field to host endianness.
 * @param byte Raw big-endian value from the DTB.
 * @return Native-endian 32-bit value.
 */
static inline uint32_t be32(uint32_t byte) {
  return __builtin_bswap32(byte);
}

/**
 * @brief Convert a 64-bit big-endian DTB field to host endianness.
 * @param byte Raw big-endian value from the DTB.
 * @return Native-endian 64-bit value.
 */
static inline uint64_t be64(uint64_t byte) {
  return __builtin_bswap64(byte);
}

/**
 * @brief Compare two null-terminated strings for equality.
 * @param str1 First string.
 * @param str2 Second string.
 * @return True when both strings contain the same characters.
 */
static bool strEq(const char* str1, const char* str2) {
  while (*str1 && *str2) {
    if (*str1 != *str2)
      return false;
    str1++;
    str2++;
  }

  return *str1 == *str2;
}

static bool strStartsWith(const char* str, const char* prefix) {
  while (*prefix) {
    if (*str != *prefix)
      return false;
    str++;
    prefix++;
  }

  return true;
}

/**
 * @brief Decode a 64-bit base/size pair from a DTB `reg` property.
 * @param data Pointer to the first 32-bit cell of the property payload.
 * @param base Output physical base address.
 * @param size Output region size in bytes.
 *
 * Uses four 32-bit big-endian cells: two address cells followed by two
 * size cells. The @c volatile qualifier prevents the compiler from
 * widening accesses on Device-nGnRnE memory before the MMU is enabled.
 */
static void readReg64(const volatile uint32_t* data, uint64_t& base, uint64_t& size) {
  base = (static_cast<uint64_t>(be32(data[0])) << 32) | static_cast<uint64_t>(be32(data[1]));
  size = (static_cast<uint64_t>(be32(data[2])) << 32) | static_cast<uint64_t>(be32(data[3]));
}

static uint64_t readU64Cells(const volatile uint32_t* data) {
  return (static_cast<uint64_t>(be32(data[0])) << 32) | static_cast<uint64_t>(be32(data[1]));
}

/**
 * @brief Advance a byte pointer to the next 4-byte DTB-aligned location.
 * @param ptr Start pointer.
 * @param bytes Number of payload bytes to skip.
 * @return Pointer rounded up to the next 32-bit boundary.
 */
static const uint32_t* alignUp(const uint8_t* ptr, uint32_t bytes) {
  uintptr_t addr = (uintptr_t)(ptr + bytes);
  addr = (addr + 3) & ~(uintptr_t)3;

  return reinterpret_cast<const uint32_t*>(addr);
}

// All DTB pointers use volatile to prevent the compiler from widening
// 32-bit reads into larger accesses. With the MMU disabled on Cortex-A76,
// the DTB resides in Device-nGnRnE memory where natural alignment must be
// respected, and the DTB format only guarantees 4-byte alignment.
MemoryMap parseDtb(uintptr_t dtb) {
  MemoryMap map = {};

  if (dtb == 0)
    return map;

  const volatile FdtHeader* hdr =
    reinterpret_cast<const volatile FdtHeader*>(dtb);

  if (be32(hdr->magic) != static_cast<uint32_t>(FDT::MAGIC))
    return map;

  map.dtbBase = dtb;
  map.dtbSize = be32(hdr->totalSize);

  const volatile uint32_t* structs =
    reinterpret_cast<const volatile uint32_t*>(dtb + be32(hdr->structOff));
  const char* strings =
    reinterpret_cast<const char*>(dtb + be32(hdr->stringsOff));

  bool foundMem = false;
  bool foundAtf = false;

  bool inReservedMemory = false;
  bool inMemory = false;
  bool inAtf = false;
  bool inChosen = false;

  int depth {};

  const volatile uint32_t* tok = structs;

  while (true) {
    uint32_t token = be32(*tok);
    tok++;

    switch (static_cast<FDT>(token)) {

      case FDT::BEGIN_NODE: {
        const char* name =
          reinterpret_cast<const char*>(const_cast<const uint32_t*>(tok));
        const uint8_t* nameB =
          reinterpret_cast<const uint8_t*>(const_cast<const uint32_t*>(tok));

        // depth == 1: inside root "/", entering a top-level node
        if (depth == 1) {
          inMemory         = strStartsWith(name, "memory");
          inReservedMemory = strEq(name, "reserved-memory");
          inChosen         = strEq(name, "chosen");
        // depth == 2: inside reserved-memory, entering a child node
        } else if (depth == 2 && inReservedMemory) {
          inAtf = strEq(name, "atf") || strEq(name, "bl31") ||
                  strEq(name, "secmon") || strEq(name, "optee") ||
                  strEq(name, "tee");
        }

        uint32_t nameLen {};
        while (nameB[nameLen] != 0)
          nameLen++;

        tok = reinterpret_cast<const volatile uint32_t*>(
          alignUp(nameB, nameLen + 1));
        depth++;
        break;
      }

      case FDT::END_NODE: {
        depth--;
        if (depth == 1) {
          inMemory = false;
          inReservedMemory = false;
          inChosen = false;
        } else if (depth == 2) {
          inAtf = false;
        }

        if (foundMem && foundAtf)
          goto done;
        break;
      }

      case FDT::PROP: {
        uint32_t        dataLen  = be32(tok[0]);
        uint32_t        nameOff  = be32(tok[1]);
        const char*     propName = strings + nameOff;
        const volatile uint32_t* propData = tok + 2;  // skip dataLen + nameOff

        if (strEq(propName, "reg") && dataLen >= 16) {
          if (inMemory && !foundMem) {
            readReg64(propData, map.memBase, map.memSize);
            foundMem = true;
          } else if (inAtf && !foundAtf) {
            readReg64(propData, map.atfBase, map.atfSize);
            foundAtf = true;
          }
        } else if (inChosen && dataLen >= 8) {
          if (strEq(propName, "linux,initrd-start")) {
            map.bootPackageBase = readU64Cells(propData);
          } else if (strEq(propName, "linux,initrd-end")) {
            uint64_t end = readU64Cells(propData);
            if (end > map.bootPackageBase)
              map.bootPackageSize = end - map.bootPackageBase;
          }
        }

        tok = reinterpret_cast<const volatile uint32_t*>(
          alignUp(reinterpret_cast<const uint8_t*>(
            const_cast<const uint32_t*>(propData)), dataLen));
        break;
      }

      case FDT::NOP:
        break;

      case FDT::END:
        goto done;

      default:
        return map;
    }
  }

done:
  map.isValid = foundMem;
  return map;
}
