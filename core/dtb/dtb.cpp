#include "dtb.h"
#include "drivers/uart/uart.h"
#include <stdint.h>

enum class FDT : uint32_t {
  MAGIC      = 0xD00DFEED,
  BEGIN_NODE = 1,
  END_NODE   = 2,
  PROP       = 3,
  NOP        = 4,
  END        = 9,
};

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

struct FdtProp {
  uint32_t dataLen;
  uint32_t nameOff;
};

static inline uint32_t be32(uint32_t byte) {
  return __builtin_bswap32(byte);
}

static inline uint64_t be64(uint64_t byte) {
  return __builtin_bswap64(byte);
}

static bool strEq(const char* str1, const char* str2) {
  while (*str1 && *str2) {
    if (*str1 != *str2)
      return false;
    str1++;
    str2++;
  }

  return true;
}

// Reads a big-endian (base, size) pair from a reg cell using 2 address and 2 size cells.
static void readReg64(const uint32_t* data, uint64_t& base, uint64_t& size) {
  base = (static_cast<uint64_t>(be32(data[0])) << 32) | static_cast<uint64_t>(be32(data[1]));
  size = (static_cast<uint64_t>(be32(data[2])) << 32) | static_cast<uint64_t>(be32(data[3]));
}

static const uint32_t* alignUp(const uint8_t* ptr, uint32_t bytes) {
  uintptr_t addr = (uintptr_t)(ptr + bytes);
  addr = (addr + 3) & ~(uintptr_t)3;

  return reinterpret_cast<const uint32_t*>(addr);
}

MemoryMap parseDtb(uintptr_t dtb) {
  MemoryMap map = {};

  if (dtb == 0)
    return map;

  const FdtHeader* hdr = reinterpret_cast<const FdtHeader*>(dtb);

  if (be32(hdr->magic) != static_cast<uint32_t>(FDT::MAGIC))
    return map;

  map.dtbBase = dtb;
  map.dtbSize = be32(hdr->totalSize);

  const uint32_t* structs =
    reinterpret_cast<const uint32_t*>(dtb + be32(hdr->structOff));
  const char* strings =
    reinterpret_cast<const char*>(dtb + be32(hdr->stringsOff));

  bool foundMem = false;
  bool foundAtf = false;

  bool inReservedMemory = false;
  bool inMemory = false;
  bool inAtf = false;

  int depth {};

  const uint32_t* tok = structs;

  while (true) {
    uint32_t token = be32(*tok);
    tok++;

    switch (static_cast<FDT>(token)) {

      case FDT::BEGIN_NODE: {
        const char* name     = reinterpret_cast<const char*>(tok);
        const uint8_t* nameB = reinterpret_cast<const uint8_t*>(tok);

        // depth == 1: inside root "/", entering a top-level node
        if (depth == 1) {
          inMemory         = strEq(name, "memory");
          inReservedMemory = strEq(name, "reserved-memory");
        // depth == 2: inside reserved-memory, entering a child node
        } else if (depth == 2 && inReservedMemory) {
          inAtf = strEq(name, "atf") || strEq(name, "bl31") ||
                  strEq(name, "secmon") || strEq(name, "optee") ||
                  strEq(name, "tee");
        }

        uint32_t nameLen {};
        while (nameB[nameLen] != 0)
          nameLen++;

        tok = alignUp(nameB, nameLen + 1);
        depth++;
        break;
      }

      case FDT::END_NODE: {
        depth--;
        if (depth == 1) {
          inMemory = false;
          inReservedMemory = false;
        } else if (depth == 2) {
          inAtf = false;
        }

        if (foundMem && foundAtf)
          goto done;
        break;
      }

      case FDT::PROP: {
        const FdtProp*  prop     = reinterpret_cast<const FdtProp*>(tok);
        uint32_t        dataLen  = be32(prop->dataLen);
        uint32_t        nameOff  = be32(prop->nameOff);
        const char*     propName = strings + nameOff;
        const uint32_t* propData = tok + 2;  // skip dataLen + nameOff

        if (strEq(propName, "reg") && dataLen >= 16) {
          if (inMemory && !foundMem) {
            readReg64(propData, map.memBase, map.memSize);
            foundMem = true;
          } else if (inAtf && !foundAtf) {
            readReg64(propData, map.atfBase, map.atfSize);
            foundAtf = true;
          }
        }

        tok = alignUp(reinterpret_cast<const uint8_t*>(propData), dataLen);
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
