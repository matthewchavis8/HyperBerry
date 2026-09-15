#pragma once

#include "lib/cpio/cpio.h"

namespace guest {
inline constexpr uint64_t GUEST_IPA_BASE { 0 };
inline constexpr uint64_t GUEST_RAM_SIZE { 256ULL * 1024 * 1024 };
inline constexpr uint64_t LINUX_KERNEL_LOAD_IPA { 0x200000 };

struct LinuxFiles {
    cpio::File kernel;
    cpio::File dtb;
    cpio::File initrd;
};

struct GuestLayout {
    uint64_t guestIpaBase;
    uint64_t guestRamSize;
    uint64_t kernelIpa;
    uint64_t kernelSize;
    uint64_t entryIpa;
    uint64_t dtbIpa;
    uint64_t dtbSize;
    uint64_t initrdIpa;
    uint64_t initrdSize;
};

struct LoadedGuest {
    uint64_t guestRamHostPa;
    uint64_t guestIpaBase;
    uint64_t guestRamSize;
    uint64_t entryIpa;
    uint64_t dtbIpa;
    uint64_t dtbHostPa;
};

enum class LoadError {
    NONE,
    INVALID_ARCHIVE,
    MISSING_KERNEL,
    MISSING_DTB,
    EMPTY_INITRD,
    GUEST_LAYOUT_OVERFLOW,
    GUEST_RAM_ALLOCATION_FAILED,
    GUEST_DTB_PATCH_FAILED,
};

struct LoadResult {
    bool isLoaded {};
    LoadError error {};
    LoadedGuest guest {};
};

LoadError ReadLinuxFiles(const cpio::Archive& archive, LinuxFiles& files);
bool CalculateGuestLayout(const LinuxFiles& files, GuestLayout& out);
LoadResult LoadLinuxGuest(const cpio::Archive& archive);
} // namespace guest
