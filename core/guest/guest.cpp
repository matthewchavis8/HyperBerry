#include "guest.h"
#include "core/dtb/fdt.h"

#include "core/mm/mmu/hostMmu/hostMmu.h"
#include "core/mm/pageTable/pageTable.h"
#include "core/mm/pmm/pmm.h"
#include "lib/strings/strings.h"

#include <memory>
#include <stddef.h>

namespace {

static constexpr uint64_t ALIGN_64K { 64 * 1024 };
static constexpr uint64_t ALIGN_2MB { 2 * 1024 * 1024 };
static constexpr uint32_t GUEST_RAM_ORDER { 16 };

static_assert(guest::GUEST_RAM_SIZE == (PAGE_SIZE << GUEST_RAM_ORDER),
        "Guest RAM size must match the PMM allocation order");

void writeBe32(uint8_t* data, uint32_t value) {
    data[0] = static_cast<uint8_t>((value >> 24) & 0xFF);
    data[1] = static_cast<uint8_t>((value >> 16) & 0xFF);
    data[2] = static_cast<uint8_t>((value >> 8) & 0xFF);
    data[3] = static_cast<uint8_t>(value & 0xFF);
}

void writeBe64Cells(uint8_t* data, uint64_t value) {
    writeBe32(data, static_cast<uint32_t>(value >> 32));
    writeBe32(data + 4, static_cast<uint32_t>(value));
}

uint64_t alignDown(uint64_t value, uint64_t alignment) {
    return value & ~(alignment - 1);
}

bool addOverflows(uint64_t a, uint64_t b) {
    return a > UINT64_MAX - b;
}

struct GuestRamDeleter {
    void operator()(uint8_t* guestRam) const noexcept {
        if (guestRam) pmm::FreePages(reinterpret_cast<uint64_t>(guestRam), GUEST_RAM_ORDER);
    }
};

guest::LoadResult loadFail(guest::LoadError error) {
    guest::LoadResult result {};
    result.error = error;
    return result;
}

void copyToGuest(uint64_t guestRamHostPa, uint64_t guestIpa, const uint8_t* source, uint64_t size) {
    uint64_t guestOffset { guestIpa - guest::GUEST_IPA_BASE };
    void* dest { HostMmu::PaToVa(guestRamHostPa + guestOffset) };
    memcpy(dest, source, static_cast<size_t>(size));
    PageTable::CleanDataCacheRange(dest, static_cast<size_t>(size));
}

uint32_t readBe32(const uint8_t* data) {
    return (uint32_t(data[0]) << 24) | (uint32_t(data[1]) << 16) | (uint32_t(data[2]) << 8) |
            data[3];
}

bool patchGuestDtb(void* dtb, const guest::GuestLayout& layout) {
    auto* base { static_cast<uint8_t*>(dtb) };
    if (layout.dtbSize < 40 || readBe32(base) != 0xd00dfeed) return false;
    uint64_t total { readBe32(base + 4) };
    uint64_t cursor { readBe32(base + 8) };
    uint64_t strings { readBe32(base + 12) };
    uint64_t stringsSize { readBe32(base + 32) };
    uint64_t structsSize { readBe32(base + 36) };
    if (total < 40 || total > layout.dtbSize || cursor < 40 || (cursor & 3) || cursor > total ||
            structsSize > total - cursor || strings < 40 || strings > total ||
            stringsSize > total - strings)
        return false;
    uint64_t end { cursor + structsSize };
    if (cursor < strings + stringsSize && strings < end) return false;
    bool inMemory {}, inChosen {}, memory {}, initrdStart {}, initrdEnd {};
    bool rootSeen {};
    unsigned depth {};
    while (cursor <= end && end - cursor >= 4) {
        uint32_t token { readBe32(base + cursor) };
        cursor += 4;
        switch (token) {
            case 1: {
                uint64_t start { cursor };
                while (cursor < end && base[cursor])
                    ++cursor;
                if (cursor == end) return false;
                const char* name { reinterpret_cast<const char*>(base + start) };
                if (!depth) {
                    if (rootSeen || *name) return false;
                    rootSeen = true;
                }
                if (depth == 1) {
                    inMemory = StrEq(name, "memory") || StrStartsWith(name, "memory@");
                    inChosen = StrEq(name, "chosen");
                }
                cursor = (cursor + 4) & ~uint64_t(3);
                ++depth;
                break;
            }
            case 2:
                if (!depth) return false;
                --depth;
                if (depth == 1) {
                    inMemory = false;
                    inChosen = false;
                }
                break;
            case 3: {
                if (!depth || end - cursor < 8) return false;
                uint64_t length { readBe32(base + cursor) };
                uint64_t nameOff { readBe32(base + cursor + 4) };
                cursor += 8;
                if (length > end - cursor || nameOff >= stringsSize) return false;
                uint64_t nameEnd { nameOff };
                while (nameEnd < stringsSize && base[strings + nameEnd])
                    ++nameEnd;
                if (nameEnd == stringsSize) return false;
                const char* name { reinterpret_cast<const char*>(base + strings + nameOff) };
                auto* value { base + cursor };
                if (depth == 2 && inMemory && StrEq(name, "reg")) {
                    if (memory || length != 16) return false;
                    writeBe64Cells(value, layout.guestIpaBase);
                    writeBe64Cells(value + 8, layout.guestRamSize);
                    memory = true;
                } else if (depth == 2 && inChosen && StrEq(name, "linux,initrd-start")) {
                    if (initrdStart || length != 8) return false;
                    writeBe64Cells(value, layout.initrdIpa);
                    initrdStart = true;
                } else if (depth == 2 && inChosen && StrEq(name, "linux,initrd-end")) {
                    if (initrdEnd || length != 8) return false;
                    writeBe64Cells(value, layout.initrdIpa + layout.initrdSize);
                    initrdEnd = true;
                }
                cursor = (cursor + length + 3) & ~uint64_t(3);
                break;
            }
            case 4:
                break;
            case 9:
                return rootSeen && !depth && memory && initrdStart && initrdEnd;
            default:
                return false;
        }
    }
    return false;
}
} // namespace

namespace guest {
LoadError ReadLinuxFiles(const cpio::Archive& archive, LinuxFiles& files) {
    files = {};
    if (archive.GetError() != cpio::Error::NONE) return LoadError::INVALID_ARCHIVE;
    if (!archive.Find("linux/Image", files.kernel) || !files.kernel.size)
        return LoadError::MISSING_KERNEL;
    if (!archive.Find("linux/guest.dtb", files.dtb) || !files.dtb.size)
        return LoadError::MISSING_DTB;
    if (archive.Find("linux/initrd", files.initrd) && !files.initrd.size)
        return LoadError::EMPTY_INITRD;
    return LoadError::NONE;
}

bool CalculateGuestLayout(const LinuxFiles& files, GuestLayout& out) {
    out = {};

    if (files.kernel.size == 0 || files.dtb.size == 0) return false;

    uint64_t guestEnd { GUEST_IPA_BASE + GUEST_RAM_SIZE };
    if (addOverflows(GUEST_IPA_BASE, GUEST_RAM_SIZE)) return false;

    uint64_t kernelEnd {};
    if (addOverflows(LINUX_KERNEL_LOAD_IPA, files.kernel.size)) return false;
    kernelEnd = LINUX_KERNEL_LOAD_IPA + files.kernel.size;

    uint64_t entryIpa { LINUX_KERNEL_LOAD_IPA };

    uint64_t highCursor { guestEnd };
    uint64_t initrdIpa {};

    if (files.initrd.size != 0) {
        if (files.initrd.size > highCursor) return false;
        initrdIpa = alignDown(highCursor - files.initrd.size, ALIGN_2MB);
        if (initrdIpa < kernelEnd) return false;
        highCursor = initrdIpa;
    }

    if (files.dtb.size > highCursor) return false;
    uint64_t dtbIpa { alignDown(highCursor - files.dtb.size, ALIGN_64K) };
    if (dtbIpa < kernelEnd) return false;

    if (entryIpa < LINUX_KERNEL_LOAD_IPA || entryIpa >= kernelEnd) return false;

    out.guestIpaBase = GUEST_IPA_BASE;
    out.guestRamSize = GUEST_RAM_SIZE;
    out.kernelIpa = LINUX_KERNEL_LOAD_IPA;
    out.kernelSize = files.kernel.size;
    out.entryIpa = entryIpa;
    out.dtbIpa = dtbIpa;
    out.dtbSize = files.dtb.size;
    out.initrdIpa = initrdIpa;
    out.initrdSize = files.initrd.size;
    return true;
}

LoadResult LoadLinuxGuest(const cpio::Archive& archive) {
    LinuxFiles files {};
    LoadError error { ReadLinuxFiles(archive, files) };
    if (error != LoadError::NONE) return loadFail(error);

    GuestLayout layout {};
    if (!CalculateGuestLayout(files, layout)) return loadFail(LoadError::GUEST_LAYOUT_OVERFLOW);

    uint64_t guestRamHostPa { pmm::AllocPages(GUEST_RAM_ORDER) };
    if (guestRamHostPa == 0) return loadFail(LoadError::GUEST_RAM_ALLOCATION_FAILED);

    std::unique_ptr<uint8_t, GuestRamDeleter> guestRam(reinterpret_cast<uint8_t*>(guestRamHostPa));

    copyToGuest(guestRamHostPa, layout.kernelIpa, files.kernel.data, files.kernel.size);
    copyToGuest(guestRamHostPa, layout.dtbIpa, files.dtb.data, files.dtb.size);
    uint64_t dtbHostPa { guestRamHostPa + (layout.dtbIpa - GUEST_IPA_BASE) };
    void* guestDtb { HostMmu::PaToVa(dtbHostPa) };
    if (!patchGuestDtb(guestDtb, layout)) return loadFail(LoadError::GUEST_DTB_PATCH_FAILED);
    PageTable::CleanDataCacheRange(guestDtb, static_cast<size_t>(layout.dtbSize));

    if (files.initrd.size != 0) {
        copyToGuest(guestRamHostPa, layout.initrdIpa, files.initrd.data, files.initrd.size);
    }

    LoadResult result {};
    result.isLoaded = true;
    result.error = LoadError::NONE;
    result.guest.guestRamHostPa = guestRamHostPa;
    result.guest.guestIpaBase = layout.guestIpaBase;
    result.guest.guestRamSize = layout.guestRamSize;
    result.guest.entryIpa = layout.entryIpa;
    result.guest.dtbIpa = layout.dtbIpa;
    result.guest.dtbHostPa = dtbHostPa;
    (void)guestRam.release();
    return result;
}

} // namespace guest
