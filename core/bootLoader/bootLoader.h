// @file bootLoader.h
// @brief Loads the Linux guest out of the firmware CPIO archive.
// @ingroup core
//
// Reads the kernel Image, guest device tree and optional initrd from the
// archive, places them in freshly allocated guest RAM following the arm64
// Linux boot protocol, and patches the guest tree's /memory and /chosen
// nodes to describe where everything landed.
#ifndef __BOOT_LOADER_H__
#define __BOOT_LOADER_H__

#include <cstdint>

#include "lib/cpio/cpio.h"

inline constexpr uint64_t GUEST_IPA_BASE { 0x40000000 };
inline constexpr uint64_t GUEST_RAM_SIZE { 256ULL * 1024 * 1024 };
inline constexpr uint64_t KERNEL_LOAD_IPA { GUEST_IPA_BASE + 0x200000 };

// @brief Views of the guest files inside the archive.
// @ingroup core
struct GuestFiles {
    cpio::File kernel; // linux/Image
    cpio::File dtb;    // linux/guest.dtb
    cpio::File initrd; // linux/initrd, empty when the archive has none
};

// @brief Where each guest file sits in guest RAM.
// @ingroup core
struct GuestLayout {
    uint64_t ramHostPa; // host physical base of guest RAM, set by BootLoader::Load()
    uint64_t kernelIpa; // also the entry point, per the arm64 Image protocol
    uint64_t kernelSize;
    uint64_t dtbIpa;
    uint64_t dtbSize;
    uint64_t initrdIpa;  // zero when there is no initrd
    uint64_t initrdSize; // zero when there is no initrd

    // @brief Translate a guest IPA inside guest RAM to its host physical address.
    // @param ipa Guest IPA between GUEST_IPA_BASE and the end of guest RAM.
    // @return The host physical address backing @p ipa.
    [[nodiscard]] constexpr uint64_t IpaToHostPa(uint64_t ipa) const {
        return ramHostPa + (ipa - GUEST_IPA_BASE);
    }
};

// @brief Loads the Linux guest from the firmware archive into guest RAM.
// @ingroup core
//
// Every step returns false on failure after logging the reason.
class BootLoader {
private:
    cpio::Archive m_archive; // copied view; the archive bytes are borrowed, not owned

public:
    // @brief Bind the loader to an archive.
    // @param archive Firmware CPIO archive. Its bytes must outlive the loader.
    explicit BootLoader(const cpio::Archive& archive);

    // @brief Find the kernel, guest tree and optional initrd in the archive.
    // @param files Receives views into the archive bytes. Cleared first.
    // @return true when the kernel and guest tree exist and nothing present is empty.
    [[nodiscard]] bool ReadFiles(GuestFiles& files) const;

    // @brief Decide where each file goes in guest RAM.
    //
    // The kernel sits at KERNEL_LOAD_IPA. The initrd is packed against the
    // top of guest RAM on a 2 MiB boundary and the tree below it on a 64 KiB
    // boundary; both must stay clear of the kernel. Leaves ramHostPa zero.
    //
    // @param files Files returned by ReadFiles().
    // @param out   Receives the layout. Cleared first.
    // @return true when every file fits without overlap or overflow.
    [[nodiscard]] static bool CalculateLayout(const GuestFiles& files, GuestLayout& out);

    // @brief Allocate guest RAM, copy the files in and patch the guest tree.
    //
    // On failure any guest RAM allocated here is returned to the PMM. On
    // success the caller owns it.
    //
    // @param out Receives the layout, including ramHostPa. Cleared first.
    // @return true when the guest is ready to run.
    [[nodiscard]] bool Load(GuestLayout& out) const;
};

#endif // !__BOOT_LOADER_H__
