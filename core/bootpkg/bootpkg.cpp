/**
 * @file bootpkg.cpp
 * @brief HyperBerry guest boot package validation.
 * @ingroup core
 */

#include "bootpkg.h"

#include "core/mm/mmu/hostMmu/hostMmu.h"
#include "core/mm/pmm/pmm.h"
#include "lib/strings/strings.h"

#include <stddef.h>

namespace {

static constexpr uint64_t ALIGN_4K = 4096;
static constexpr uint64_t ALIGN_64K = 64 * 1024;
static constexpr uint64_t ALIGN_2MB = 2 * 1024 * 1024;
static constexpr uint32_t GUEST_RAM_ORDER = 16;

static_assert(bootpkg::GUEST_RAM_SIZE == (PAGE_SIZE << GUEST_RAM_ORDER),
              "Guest RAM size must match the PMM allocation order");

static constexpr uint64_t OFF_MAGIC = 0;
static constexpr uint64_t OFF_VERSION = 4;
static constexpr uint64_t OFF_HEADER_SIZE = 6;
static constexpr uint64_t OFF_TOTAL_SIZE = 8;
static constexpr uint64_t OFF_HEADER_CRC32 = 16;
static constexpr uint64_t OFF_PAYLOAD_CRC32 = 20;
static constexpr uint64_t OFF_BOOT_PROTOCOL = 24;
static constexpr uint64_t OFF_FLAGS = 28;
static constexpr uint64_t OFF_KERNEL_OFFSET = 32;
static constexpr uint64_t OFF_KERNEL_SIZE = 40;
static constexpr uint64_t OFF_DTB_OFFSET = 48;
static constexpr uint64_t OFF_DTB_SIZE = 56;
static constexpr uint64_t OFF_INITRD_OFFSET = 64;
static constexpr uint64_t OFF_INITRD_SIZE = 72;
static constexpr uint64_t OFF_ENTRY_OFFSET = 80;
static constexpr uint64_t OFF_BUILD_ID = 88;

static constexpr uint64_t CHECKSUM_FIELDS_START = OFF_HEADER_CRC32;
static constexpr uint64_t CHECKSUM_FIELDS_END = OFF_PAYLOAD_CRC32 + sizeof(uint32_t);

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

uint16_t readLe16(const uint8_t* data, uint64_t off) {
  return static_cast<uint16_t>(data[off])
       | static_cast<uint16_t>(data[off + 1] << 8);
}

uint32_t readLe32(const uint8_t* data, uint64_t off) {
  return static_cast<uint32_t>(data[off])
       | (static_cast<uint32_t>(data[off + 1]) << 8)
       | (static_cast<uint32_t>(data[off + 2]) << 16)
       | (static_cast<uint32_t>(data[off + 3]) << 24);
}

uint64_t readLe64(const uint8_t* data, uint64_t off) {
  return static_cast<uint64_t>(readLe32(data, off))
       | (static_cast<uint64_t>(readLe32(data, off + 4)) << 32);
}

uint32_t be32(uint32_t byte) {
  return __builtin_bswap32(byte);
}

void writeBe32(uint8_t* data, uint32_t value) {
  data[0] = static_cast<uint8_t>((value >> 24) & 0xFF);
  data[1] = static_cast<uint8_t>((value >> 16) & 0xFF);
  data[2] = static_cast<uint8_t>((value >>  8) & 0xFF);
  data[3] = static_cast<uint8_t>( value        & 0xFF);
}

void writeBe64Cells(uint8_t* data, uint64_t value) {
  writeBe32(data, static_cast<uint32_t>(value >> 32));
  writeBe32(data + 4, static_cast<uint32_t>(value));
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

uint64_t align4k(uint64_t value) {
  return (value + ALIGN_4K - 1) & ~(ALIGN_4K - 1);
}

uint64_t alignDown(uint64_t value, uint64_t alignment) {
  return value & ~(alignment - 1);
}

bool addOverflows(uint64_t a, uint64_t b) {
  return a > UINT64_MAX - b;
}

bool rangeInBounds(uint64_t offset, uint64_t componentSize, uint64_t totalSize) {
  if (componentSize == 0)
    return false;
  if (offset >= totalSize)
    return false;
  if (addOverflows(offset, componentSize))
    return false;
  return offset + componentSize <= totalSize;
}

uint32_t crc32Update(uint32_t crc, uint8_t byte) {
  crc ^= byte;
  for (uint32_t bit {}; bit < 8; bit++) {
    uint32_t mask = 0U - (crc & 1U);
    crc = (crc >> 1) ^ (0xEDB88320U & mask);
  }

  return crc;
}

uint32_t headerCrc32(const uint8_t* data) {
  uint32_t crc = 0xFFFFFFFFU;

  for (uint64_t i {}; i < bootpkg::HV_GUEST_BOOT_PKG_HEADER_SIZE; i++) {
    uint8_t byte = data[i];
    if (i >= CHECKSUM_FIELDS_START && i < CHECKSUM_FIELDS_END)
      byte = 0;
    crc = crc32Update(crc, byte);
  }

  return ~crc;
}

bootpkg::ValidateResult fail(bootpkg::ValidateError error) {
  bootpkg::ValidateResult result = {};
  result.error = error;
  return result;
}

bootpkg::LoadResult loadFail(bootpkg::LoadError error,
                             bootpkg::ValidateError validateError = bootpkg::ValidateError::None) {
  bootpkg::LoadResult result = {};
  result.error = error;
  result.validateError = validateError;
  return result;
}

void copyToGuest(uint64_t guestRamHostPa, uint64_t guestIpa,
                 const uint8_t* source, uint64_t size) {
  uint64_t guestOffset = guestIpa - bootpkg::GUEST_IPA_BASE;
  void* dest = HostMmu::paToVa(guestRamHostPa + guestOffset);
  memcpy(dest, source, static_cast<size_t>(size));
}

bool patchGuestDtb(void* dtb, const bootpkg::GuestLayout& layout) {
  if (dtb == nullptr)
    return false;

  auto* hdr = static_cast<FdtHeader*>(dtb);
  if (be32(hdr->magic) != static_cast<uint32_t>(FDT::MAGIC))
    return false;

  auto* base = static_cast<uint8_t*>(dtb);
  auto* tok = reinterpret_cast<uint32_t*>(base + be32(hdr->structOff));
  const char* strings = reinterpret_cast<const char*>(base + be32(hdr->stringsOff));

  bool inMemory = false;
  bool inChosen = false;
  bool patchedMemory = false;
  bool patchedInitrdStart = false;
  bool patchedInitrdEnd = false;
  int depth {};

  while (true) {
    uint32_t token = be32(*tok);
    tok++;

    switch (static_cast<FDT>(token)) {
      case FDT::BEGIN_NODE: {
        const char* name = reinterpret_cast<const char*>(tok);
        auto* nameB = reinterpret_cast<uint8_t*>(tok);

        if (depth == 1) {
          inMemory = strStartsWith(name, "memory");
          inChosen = strEq(name, "chosen");
        }

        uint32_t nameLen {};
        while (nameB[nameLen] != 0)
          nameLen++;

        tok = alignStruct(nameB, nameLen + 1);
        depth++;
        break;
      }

      case FDT::END_NODE:
        depth--;
        if (depth == 1) {
          inMemory = false;
          inChosen = false;
        }
        break;

      case FDT::PROP: {
        uint32_t dataLen = be32(tok[0]);
        uint32_t nameOff = be32(tok[1]);
        const char* propName = strings + nameOff;
        auto* propData = reinterpret_cast<uint8_t*>(tok + 2);

        if (inMemory && strEq(propName, "reg")) {
          if (dataLen != 16)
            return false;
          writeBe64Cells(propData, layout.guestIpaBase);
          writeBe64Cells(propData + 8, layout.guestRamSize);
          patchedMemory = true;
        } else if (inChosen && strEq(propName, "linux,initrd-start")) {
          if (dataLen != 8)
            return false;
          writeBe64Cells(propData, layout.initrdIpa);
          patchedInitrdStart = true;
        } else if (inChosen && strEq(propName, "linux,initrd-end")) {
          if (dataLen != 8)
            return false;
          uint64_t initrdEnd = layout.initrdSize == 0
                             ? 0
                             : layout.initrdIpa + layout.initrdSize;
          writeBe64Cells(propData, initrdEnd);
          patchedInitrdEnd = true;
        }

        tok = alignStruct(propData, dataLen);
        break;
      }

      case FDT::NOP:
        break;

      case FDT::END:
        return patchedMemory && patchedInitrdStart && patchedInitrdEnd;

      default:
        return false;
    }
  }
}

} // namespace

namespace bootpkg {

uint32_t crc32(const void* data, uint64_t size) {
  if (data == nullptr && size != 0)
    return 0;

  const auto* bytes = static_cast<const uint8_t*>(data);
  uint32_t crc = 0xFFFFFFFFU;

  for (uint64_t i {}; i < size; i++) {
    crc = crc32Update(crc, bytes[i]);
  }

  return ~crc;
}

ValidateResult validate(const void* package, uint64_t size) {
  if (package == nullptr)
    return fail(ValidateError::NullPackage);
  if (size < HV_GUEST_BOOT_PKG_HEADER_SIZE)
    return fail(ValidateError::TooSmall);

  const auto* bytes = static_cast<const uint8_t*>(package);

  if (readLe32(bytes, OFF_MAGIC) != HV_GUEST_BOOT_PKG_MAGIC)
    return fail(ValidateError::BadMagic);
  if (readLe16(bytes, OFF_VERSION) != HV_GUEST_BOOT_PKG_VERSION)
    return fail(ValidateError::BadVersion);
  if (readLe16(bytes, OFF_HEADER_SIZE) != HV_GUEST_BOOT_PKG_HEADER_SIZE)
    return fail(ValidateError::BadHeaderSize);

  PackageView view = {};
  view.totalSize = readLe64(bytes, OFF_TOTAL_SIZE);
  view.bootProtocol = readLe32(bytes, OFF_BOOT_PROTOCOL);
  view.flags = readLe32(bytes, OFF_FLAGS);
  view.kernelOffset = readLe64(bytes, OFF_KERNEL_OFFSET);
  view.kernelSize = readLe64(bytes, OFF_KERNEL_SIZE);
  view.dtbOffset = readLe64(bytes, OFF_DTB_OFFSET);
  view.dtbSize = readLe64(bytes, OFF_DTB_SIZE);
  view.initrdOffset = readLe64(bytes, OFF_INITRD_OFFSET);
  view.initrdSize = readLe64(bytes, OFF_INITRD_SIZE);
  view.entryOffset = readLe64(bytes, OFF_ENTRY_OFFSET);
  view.buildId = reinterpret_cast<const char*>(bytes + OFF_BUILD_ID);

  if (view.totalSize < HV_GUEST_BOOT_PKG_HEADER_SIZE || view.totalSize > size)
    return fail(ValidateError::BadTotalSize);

  uint32_t expectedHeaderCrc = readLe32(bytes, OFF_HEADER_CRC32);
  if (headerCrc32(bytes) != expectedHeaderCrc)
    return fail(ValidateError::BadHeaderCrc);

  uint32_t expectedPayloadCrc = readLe32(bytes, OFF_PAYLOAD_CRC32);
  if (crc32(bytes + HV_GUEST_BOOT_PKG_HEADER_SIZE, view.totalSize - HV_GUEST_BOOT_PKG_HEADER_SIZE) != expectedPayloadCrc)
    return fail(ValidateError::BadPayloadCrc);

  if (view.bootProtocol != HV_GUEST_BOOT_PKG_BOOT_PROTOCOL_LINUX_ARM64)
    return fail(ValidateError::UnsupportedBootProtocol);
  if ((view.flags & ~HV_GUEST_BOOT_PKG_KNOWN_FLAGS) != 0)
    return fail(ValidateError::UnknownFlags);
  if (view.kernelSize == 0)
    return fail(ValidateError::MissingKernel);
  if (view.dtbSize == 0)
    return fail(ValidateError::MissingDtb);

  bool hasInitrdFlag = (view.flags & HV_GUEST_BOOT_PKG_FLAG_INITRD_PRESENT) != 0;
  bool hasInitrd = view.initrdSize != 0;
  if (hasInitrdFlag != hasInitrd)
    return fail(ValidateError::BadInitrdFlag);
  if (!hasInitrd && view.initrdOffset != 0)
    return fail(ValidateError::BadInitrdFlag);

  if (view.kernelOffset != HV_GUEST_BOOT_PKG_HEADER_SIZE)
    return fail(ValidateError::BadKernelOffset);

  uint64_t expectedDtbOffset = align4k(view.kernelOffset + view.kernelSize);
  if (addOverflows(view.kernelOffset, view.kernelSize)
      || view.dtbOffset != expectedDtbOffset) {
    return fail(ValidateError::BadDtbOffset);
  }

  if (!rangeInBounds(view.kernelOffset, view.kernelSize, view.totalSize)
      || !rangeInBounds(view.dtbOffset, view.dtbSize, view.totalSize)) {
    return fail(ValidateError::ComponentOutOfBounds);
  }

  uint64_t expectedTotalSize {};
  if (hasInitrd) {
    uint64_t expectedInitrdOffset = align4k(view.dtbOffset + view.dtbSize);
    if (addOverflows(view.dtbOffset, view.dtbSize)
        || view.initrdOffset != expectedInitrdOffset) {
      return fail(ValidateError::BadInitrdOffset);
    }
    if (!rangeInBounds(view.initrdOffset, view.initrdSize, view.totalSize))
      return fail(ValidateError::ComponentOutOfBounds);
    if (addOverflows(view.initrdOffset, view.initrdSize))
      return fail(ValidateError::BadTotalLayout);
    expectedTotalSize = align4k(view.initrdOffset + view.initrdSize);
  } else {
    if (addOverflows(view.dtbOffset, view.dtbSize))
      return fail(ValidateError::BadTotalLayout);
    expectedTotalSize = align4k(view.dtbOffset + view.dtbSize);
  }

  if (expectedTotalSize != view.totalSize)
    return fail(ValidateError::BadTotalLayout);

  ValidateResult result = {};
  result.isValid = true;
  result.error = ValidateError::None;
  result.package = view;
  return result;
}

bool calculateGuestLayout(const PackageView& package, GuestLayout& out) {
  out = {};

  if (package.kernelSize == 0 || package.dtbSize == 0)
    return false;

  uint64_t guestEnd = GUEST_IPA_BASE + GUEST_RAM_SIZE;
  if (addOverflows(GUEST_IPA_BASE, GUEST_RAM_SIZE))
    return false;

  uint64_t kernelEnd {};
  if (addOverflows(LINUX_KERNEL_LOAD_IPA, package.kernelSize))
    return false;
  kernelEnd = LINUX_KERNEL_LOAD_IPA + package.kernelSize;

  if (addOverflows(LINUX_KERNEL_LOAD_IPA, package.entryOffset))
    return false;
  uint64_t entryIpa = LINUX_KERNEL_LOAD_IPA + package.entryOffset;

  uint64_t highCursor = guestEnd;
  uint64_t initrdIpa {};

  if (package.initrdSize != 0) {
    if (package.initrdSize > highCursor)
      return false;
    initrdIpa = alignDown(highCursor - package.initrdSize, ALIGN_2MB);
    if (initrdIpa < kernelEnd)
      return false;
    highCursor = initrdIpa;
  }

  if (package.dtbSize > highCursor)
    return false;
  uint64_t dtbIpa = alignDown(highCursor - package.dtbSize, ALIGN_64K);
  if (dtbIpa < kernelEnd)
    return false;

  if (entryIpa < LINUX_KERNEL_LOAD_IPA || entryIpa >= kernelEnd)
    return false;

  out.guestIpaBase = GUEST_IPA_BASE;
  out.guestRamSize = GUEST_RAM_SIZE;
  out.kernelIpa = LINUX_KERNEL_LOAD_IPA;
  out.kernelSize = package.kernelSize;
  out.entryIpa = entryIpa;
  out.dtbIpa = dtbIpa;
  out.dtbSize = package.dtbSize;
  out.initrdIpa = initrdIpa;
  out.initrdSize = package.initrdSize;
  return true;
}

LoadResult loadLinuxGuest(const MemoryMap& map) {
  if (map.bootPackageBase == 0 || map.bootPackageSize == 0)
    return loadFail(LoadError::MissingFirmwarePackage);

  const auto* packageBytes =
    static_cast<const uint8_t*>(HostMmu::paToVa(map.bootPackageBase));

  ValidateResult validated = validate(packageBytes, map.bootPackageSize);
  if (!validated.isValid)
    return loadFail(LoadError::InvalidPackage, validated.error);

  GuestLayout layout = {};
  if (!calculateGuestLayout(validated.package, layout))
    return loadFail(LoadError::GuestLayoutOverflow);

  uint64_t guestRamHostPa = pmm::allocPages(GUEST_RAM_ORDER);
  if (guestRamHostPa == 0)
    return loadFail(LoadError::GuestRamAllocationFailed);

  copyToGuest(guestRamHostPa, layout.kernelIpa,
              packageBytes + validated.package.kernelOffset,
              validated.package.kernelSize);
  copyToGuest(guestRamHostPa, layout.dtbIpa,
              packageBytes + validated.package.dtbOffset,
              validated.package.dtbSize);
  void* guestDtb = HostMmu::paToVa(guestRamHostPa + (layout.dtbIpa - GUEST_IPA_BASE));
  if (!patchGuestDtb(guestDtb, layout))
    return loadFail(LoadError::GuestDtbPatchFailed);

  if (validated.package.initrdSize != 0) {
    copyToGuest(guestRamHostPa, layout.initrdIpa,
                packageBytes + validated.package.initrdOffset,
                validated.package.initrdSize);
  }

  LoadResult result = {};
  result.isLoaded = true;
  result.error = LoadError::None;
  result.validateError = ValidateError::None;
  result.guest.guestRamHostPa = guestRamHostPa;
  result.guest.guestIpaBase = layout.guestIpaBase;
  result.guest.guestRamSize = layout.guestRamSize;
  result.guest.entryIpa = layout.entryIpa;
  result.guest.dtbIpa = layout.dtbIpa;
  return result;
}

} // namespace bootpkg
