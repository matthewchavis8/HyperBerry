/**
 * @file test_bootpkg.cpp
 * @brief Integration tests for firmware-loaded guest boot packages.
 */

#include "core/bootpkg/bootpkg.h"
#include "core/mm/mmu/hostMmu/hostMmu.h"
#include "core/mm/pmm/pmm.h"
#include "tests/integration/suite.h"

namespace {
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

uint32_t be32(uint32_t value) {
  return __builtin_bswap32(value);
}

bool strEq(const char* str1, const char* str2) {
  while (*str1 && *str2) {
    if (*str1 != *str2)
      return false;
    str1++;
    str2++;
  }

  return *str1 == *str2;
}

bool strStartsWith(const char* str, const char* prefix) {
  while (*prefix) {
    if (*str != *prefix)
      return false;
    str++;
    prefix++;
  }

  return true;
}

uint32_t* alignStruct(uint8_t* ptr, uint32_t bytes) {
  uintptr_t addr = reinterpret_cast<uintptr_t>(ptr + bytes);
  addr = (addr + 3) & ~(uintptr_t)3;
  return reinterpret_cast<uint32_t*>(addr);
}

uint64_t readBe64Cells(const uint8_t* data) {
  const auto* cells = reinterpret_cast<const uint32_t*>(data);
  return (static_cast<uint64_t>(be32(cells[0])) << 32)
       | static_cast<uint64_t>(be32(cells[1]));
}

uint8_t* findPropData(void* dtb, const char* wanted) {
  auto* hdr = static_cast<FdtHeader*>(dtb);
  if (be32(hdr->magic) != static_cast<uint32_t>(FDT::MAGIC))
    return nullptr;

  auto* base = static_cast<uint8_t*>(dtb);
  auto* tok = reinterpret_cast<uint32_t*>(base + be32(hdr->structOff));
  const char* strings = reinterpret_cast<const char*>(base + be32(hdr->stringsOff));

  while (true) {
    uint32_t token = be32(*tok);
    tok++;

    switch (static_cast<FDT>(token)) {
      case FDT::BEGIN_NODE: {
        auto* name = reinterpret_cast<uint8_t*>(tok);
        uint32_t nameLen {};
        while (name[nameLen] != 0)
          nameLen++;
        tok = alignStruct(name, nameLen + 1);
        break;
      }

      case FDT::END_NODE:
      case FDT::NOP:
        break;

      case FDT::PROP: {
        uint32_t dataLen = be32(tok[0]);
        uint32_t nameOff = be32(tok[1]);
        auto* propData = reinterpret_cast<uint8_t*>(tok + 2);
        if (strEq(strings + nameOff, wanted))
          return propData;
        tok = alignStruct(propData, dataLen);
        break;
      }

      case FDT::END:
      default:
        return nullptr;
    }
  }
}

uint8_t* findMemoryRegData(void* dtb) {
  auto* hdr = static_cast<FdtHeader*>(dtb);
  if (be32(hdr->magic) != static_cast<uint32_t>(FDT::MAGIC))
    return nullptr;

  auto* base = static_cast<uint8_t*>(dtb);
  auto* tok = reinterpret_cast<uint32_t*>(base + be32(hdr->structOff));
  const char* strings = reinterpret_cast<const char*>(base + be32(hdr->stringsOff));
  bool inMemory = false;
  int depth {};

  while (true) {
    uint32_t token = be32(*tok);
    tok++;

    switch (static_cast<FDT>(token)) {
      case FDT::BEGIN_NODE: {
        const char* name = reinterpret_cast<const char*>(tok);
        auto* nameBytes = reinterpret_cast<uint8_t*>(tok);
        if (depth == 1)
          inMemory = strStartsWith(name, "memory");

        uint32_t nameLen {};
        while (nameBytes[nameLen] != 0)
          nameLen++;
        tok = alignStruct(nameBytes, nameLen + 1);
        depth++;
        break;
      }

      case FDT::END_NODE:
        depth--;
        if (depth == 1)
          inMemory = false;
        break;

      case FDT::NOP:
        break;

      case FDT::PROP: {
        uint32_t dataLen = be32(tok[0]);
        uint32_t nameOff = be32(tok[1]);
        auto* propData = reinterpret_cast<uint8_t*>(tok + 2);
        if (inMemory && strEq(strings + nameOff, "reg"))
          return propData;
        tok = alignStruct(propData, dataLen);
        break;
      }

      case FDT::END:
      default:
        return nullptr;
    }
  }
}

const uint8_t* packageBytes(const MemoryMap& map) {
  return static_cast<const uint8_t*>(HostMmu::paToVa(map.bootPackageBase));
}
} // namespace

static bool test_firmware_package_region_present() {
  const MemoryMap& map = TestRunner::bootMemoryMap();
  return map.bootPackageBase != 0 && map.bootPackageSize != 0;
}

static bool test_firmware_package_validates() {
  const MemoryMap& map = TestRunner::bootMemoryMap();
  if (map.bootPackageBase == 0 || map.bootPackageSize == 0)
    return false;

  bootpkg::ValidateResult result =
    bootpkg::validate(packageBytes(map), map.bootPackageSize);

  return result.isValid
      && result.error == bootpkg::ValidateError::None
      && result.package.bootProtocol == bootpkg::HV_GUEST_BOOT_PKG_BOOT_PROTOCOL_LINUX_ARM64;
}

static bool test_load_linux_guest_from_firmware_package() {
  const MemoryMap& map = TestRunner::bootMemoryMap();
  bootpkg::ValidateResult validated =
    bootpkg::validate(packageBytes(map), map.bootPackageSize);
  if (!validated.isValid)
    return false;

  bootpkg::GuestLayout layout = {};
  if (!bootpkg::calculateGuestLayout(validated.package, layout))
    return false;

  bootpkg::LoadResult loaded = bootpkg::loadLinuxGuest(map);
  if (!loaded.isLoaded)
    return false;

  const uint8_t* kernel =
    static_cast<const uint8_t*>(HostMmu::paToVa(
      loaded.guest.guestRamHostPa + (layout.kernelIpa - layout.guestIpaBase)));
  const uint8_t* packageKernel = packageBytes(map) + validated.package.kernelOffset;

  bool copied = kernel[0] == packageKernel[0]
             && kernel[validated.package.kernelSize - 1]
             == packageKernel[validated.package.kernelSize - 1];
  bool metadata = loaded.error == bootpkg::LoadError::None
               && loaded.guest.guestIpaBase == layout.guestIpaBase
               && loaded.guest.guestRamSize == layout.guestRamSize
               && loaded.guest.entryIpa == layout.entryIpa
               && loaded.guest.dtbIpa == layout.dtbIpa;

  pmm::freePages(loaded.guest.guestRamHostPa, 16);
  return copied && metadata;
}

static bool test_load_patches_guest_dtb() {
  const MemoryMap& map = TestRunner::bootMemoryMap();
  bootpkg::ValidateResult validated =
    bootpkg::validate(packageBytes(map), map.bootPackageSize);
  if (!validated.isValid)
    return false;

  bootpkg::GuestLayout layout = {};
  if (!bootpkg::calculateGuestLayout(validated.package, layout))
    return false;

  bootpkg::LoadResult loaded = bootpkg::loadLinuxGuest(map);
  if (!loaded.isLoaded)
    return false;

  void* dtb = HostMmu::paToVa(
    loaded.guest.guestRamHostPa + (layout.dtbIpa - layout.guestIpaBase));
  uint8_t* memoryReg = findMemoryRegData(dtb);
  uint8_t* initrdStart = findPropData(dtb, "linux,initrd-start");
  uint8_t* initrdEnd = findPropData(dtb, "linux,initrd-end");

  bool patched = memoryReg != nullptr
              && initrdStart != nullptr
              && initrdEnd != nullptr
              && readBe64Cells(memoryReg) == layout.guestIpaBase
              && readBe64Cells(memoryReg + 8) == layout.guestRamSize
              && readBe64Cells(initrdStart) == layout.initrdIpa
              && readBe64Cells(initrdEnd) == layout.initrdIpa + layout.initrdSize;

  pmm::freePages(loaded.guest.guestRamHostPa, 16);
  return patched;
}

static const TestCase kBootPkgCases[] = {
    {"firmware_package_region_present",        test_firmware_package_region_present},
    {"firmware_package_validates",             test_firmware_package_validates},
    {"load_linux_guest_from_firmware_package", test_load_linux_guest_from_firmware_package},
    {"load_patches_guest_dtb",                 test_load_patches_guest_dtb},
};

static const TestSuite kBootPkgSuite = {
    "BootPkgHarness",
    kBootPkgCases,
    4,
};

REGISTER_SUITE(kBootPkgSuite);
