#include <gtest/gtest.h>

#include "core/guest/guest.h"
#include "core/deviceTree/deviceTree.h"
#include "tests/unit/cpio/fixture.h"
#include "core/mm/pmm/pmm.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace {

static uint64_t gAllocPagesReturn { 0x10000000ULL };
static uint32_t gAllocPagesOrder { UINT32_MAX };

static constexpr uint64_t kKernelSize { 0x1800 };
static constexpr uint64_t kDtbSize { 0x800 };
static constexpr uint64_t kInitrdSize { 0x2800 };

void pushBE32(std::vector<uint8_t>& data, uint32_t value) {
    data.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    data.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    data.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    data.push_back(static_cast<uint8_t>(value & 0xFF));
}

uint32_t readBe32(const std::vector<uint8_t>& data, uint64_t off) {
    return (static_cast<uint32_t>(data[off]) << 24) | (static_cast<uint32_t>(data[off + 1]) << 16) |
            (static_cast<uint32_t>(data[off + 2]) << 8) | static_cast<uint32_t>(data[off + 3]);
}

uint64_t readBe64Cells(const std::vector<uint8_t>& data, uint64_t off) {
    return (static_cast<uint64_t>(readBe32(data, off)) << 32) |
            static_cast<uint64_t>(readBe32(data, off + 4));
}

void fillRange(std::vector<uint8_t>& data, uint64_t off, uint64_t size, uint8_t value) {
    for (uint64_t i {}; i < size; i++) {
        data[off + i] = static_cast<uint8_t>(value + i);
    }
}

class DtbBuilder {
    std::vector<uint8_t> m_structs;
    std::vector<char> m_strings;

public:
    uint32_t AddString(const char* str) {
        uint32_t off { static_cast<uint32_t>(m_strings.size()) };
        while (*str) {
            m_strings.push_back(*str);
            str++;
        }
        m_strings.push_back('\0');
        return off;
    }

    void BeginNode(const char* name) {
        pushBE32(m_structs, 1);
        while (*name) {
            m_structs.push_back(static_cast<uint8_t>(*name));
            name++;
        }
        m_structs.push_back(0);
        while ((m_structs.size() & 3U) != 0)
            m_structs.push_back(0);
    }

    void EndNode() { pushBE32(m_structs, 2); }

    void PropReg64(uint32_t nameOff, uint64_t base, uint64_t size) {
        pushBE32(m_structs, 3);
        pushBE32(m_structs, 16);
        pushBE32(m_structs, nameOff);
        pushBE32(m_structs, static_cast<uint32_t>(base >> 32));
        pushBE32(m_structs, static_cast<uint32_t>(base));
        pushBE32(m_structs, static_cast<uint32_t>(size >> 32));
        pushBE32(m_structs, static_cast<uint32_t>(size));
    }

    void PropU64Cells(uint32_t nameOff, uint64_t value) {
        pushBE32(m_structs, 3);
        pushBE32(m_structs, 8);
        pushBE32(m_structs, nameOff);
        pushBE32(m_structs, static_cast<uint32_t>(value >> 32));
        pushBE32(m_structs, static_cast<uint32_t>(value));
    }

    void end() { pushBE32(m_structs, 9); }

    std::vector<uint8_t> Build() {
        constexpr uint32_t headerSize { 40 };
        constexpr uint32_t reserveSize { 16 };

        uint32_t structOff { headerSize + reserveSize };
        uint32_t structSize { static_cast<uint32_t>(m_structs.size()) };
        uint32_t stringsOff { structOff + structSize };
        uint32_t stringsSize { static_cast<uint32_t>(m_strings.size()) };
        uint32_t totalSize { stringsOff + stringsSize };

        std::vector<uint8_t> blob(totalSize, 0);
        auto writeHeader = [&](uint32_t off, uint32_t value) {
            blob[off + 0] = static_cast<uint8_t>((value >> 24) & 0xFF);
            blob[off + 1] = static_cast<uint8_t>((value >> 16) & 0xFF);
            blob[off + 2] = static_cast<uint8_t>((value >> 8) & 0xFF);
            blob[off + 3] = static_cast<uint8_t>(value & 0xFF);
        };

        writeHeader(0, 0xD00DFEED);
        writeHeader(4, totalSize);
        writeHeader(8, structOff);
        writeHeader(12, stringsOff);
        writeHeader(16, headerSize);
        writeHeader(20, 17);
        writeHeader(24, 16);
        writeHeader(28, 0);
        writeHeader(32, stringsSize);
        writeHeader(36, structSize);

        std::copy(m_structs.begin(), m_structs.end(), blob.begin() + structOff);
        std::copy(m_strings.begin(), m_strings.end(), blob.begin() + stringsOff);
        return blob;
    }
};

std::vector<uint8_t> buildGuestDtb(bool includeInitrdPlaceholders = true) {
    DtbBuilder b;
    uint32_t reg { b.AddString("reg") };
    uint32_t initrdStart { b.AddString("linux,initrd-start") };
    uint32_t initrdEnd { b.AddString("linux,initrd-end") };

    b.BeginNode("");
    b.BeginNode("memory@0");
    b.PropReg64(reg, 0, 0);
    b.EndNode();
    b.BeginNode("chosen");
    if (includeInitrdPlaceholders) {
        b.PropU64Cells(initrdStart, 0);
        b.PropU64Cells(initrdEnd, 0);
    }
    b.EndNode();
    b.EndNode();
    b.end();

    auto dtb { b.Build() };
    dtb.resize(kDtbSize, 0);
    return dtb;
}

bool strEq(const char* str1, const char* str2) {
    while (*str1 && *str2) {
        if (*str1 != *str2) return false;
        str1++;
        str2++;
    }

    return *str1 == *str2;
}

uint64_t alignStruct(uint64_t off, uint32_t bytes) {
    return (off + bytes + 3ULL) & ~3ULL;
}

uint64_t findPropData(std::vector<uint8_t>& dtb, const char* wanted) {
    uint64_t tok { readBe32(dtb, 8) };
    const char* strings { reinterpret_cast<const char*>(dtb.data() + readBe32(dtb, 12)) };

    while (true) {
        uint32_t token { readBe32(dtb, tok) };
        tok += 4;

        if (token == 1) {
            uint64_t name { tok };
            while (dtb[tok] != 0)
                tok++;
            tok = alignStruct(name, static_cast<uint32_t>(tok - name + 1));
        } else if (token == 2 || token == 4) {
            continue;
        } else if (token == 3) {
            uint32_t dataLen { readBe32(dtb, tok) };
            uint32_t nameOff { readBe32(dtb, tok + 4) };
            uint64_t dataOff { tok + 8 };
            if (strEq(strings + nameOff, wanted)) return dataOff;
            tok = alignStruct(dataOff, dataLen);
        } else if (token == 9) {
            return 0;
        } else {
            return 0;
        }
    }
}

std::vector<uint8_t> buildArchive(bool withInitrd = true, bool placeholders = true) {
    std::vector<uint8_t> bytes;
    std::vector<uint8_t> kernel(kKernelSize);
    fillRange(kernel, 0, kernel.size(), 0x10);
    fixture::Entry(bytes, "linux/Image", kernel);
    fixture::Entry(bytes, "linux/guest.dtb", buildGuestDtb(placeholders));
    if (withInitrd) {
        std::vector<uint8_t> initrd(kInitrdSize);
        fillRange(initrd, 0, initrd.size(), 0x50);
        fixture::Entry(bytes, "linux/initrd", initrd);
    }
    fixture::Finish(bytes);
    return bytes;
}

unsigned freed {};
} // namespace

namespace pmm {
uint64_t AllocPages(uint32_t order) {
    gAllocPagesOrder = order;
    return gAllocPagesReturn;
}
void FreePages(uint64_t, uint32_t) {
    ++freed;
}
} // namespace pmm

namespace PageTable {
void CleanDataCacheRange(const void*, size_t) {}
} // namespace PageTable

TEST(Guest, ResolvesFilesWithAndWithoutInitrd) {
    for (bool withInitrd : { false, true }) {
        auto bytes { buildArchive(withInitrd) };
        cpio::Archive archive { bytes.data(), bytes.size() };
        guest::LinuxFiles files {};
        ASSERT_EQ(guest::ReadLinuxFiles(archive, files), guest::LoadError::NONE);
        EXPECT_EQ(files.kernel.size, kKernelSize);
        EXPECT_EQ(files.dtb.size, kDtbSize);
        EXPECT_EQ(files.initrd.size, withInitrd ? kInitrdSize : 0);
        guest::GuestLayout layout {};
        ASSERT_TRUE(guest::CalculateGuestLayout(files, layout));
        EXPECT_EQ(layout.entryIpa, guest::LINUX_KERNEL_LOAD_IPA);
        EXPECT_EQ(layout.dtbIpa % 65536, 0);
        EXPECT_EQ(layout.initrdIpa % (2 * 1024 * 1024), 0);
        EXPECT_LE(layout.kernelIpa + layout.kernelSize, layout.dtbIpa);
        EXPECT_LE(layout.dtbIpa + layout.dtbSize,
                withInitrd ? layout.initrdIpa : guest::GUEST_IPA_BASE + guest::GUEST_RAM_SIZE);
    }
}

TEST(Guest, RejectsMissingOrEmptyRequiredFiles) {
    for (bool kernel : { false, true }) {
        std::vector<uint8_t> bytes;
        fixture::Entry(bytes, kernel ? "linux/Image" : "linux/guest.dtb", { 1 });
        fixture::Finish(bytes);
        cpio::Archive archive { bytes.data(), bytes.size() };
        EXPECT_EQ(guest::LoadLinuxGuest(archive).error,
                kernel ? guest::LoadError::MISSING_DTB : guest::LoadError::MISSING_KERNEL);
    }
    std::vector<uint8_t> bytes;
    fixture::Entry(bytes, "linux/Image");
    fixture::Finish(bytes);
    EXPECT_EQ(guest::LoadLinuxGuest(cpio::Archive(bytes.data(), bytes.size())).error,
            guest::LoadError::MISSING_KERNEL);
}

TEST(Guest, RejectsOverflowingLayouts) {
    guest::LinuxFiles files { { nullptr, UINT64_MAX }, { nullptr, 64 }, {} };
    guest::GuestLayout layout {};
    EXPECT_FALSE(guest::CalculateGuestLayout(files, layout));
    files.kernel.size = guest::GUEST_RAM_SIZE;
    EXPECT_FALSE(guest::CalculateGuestLayout(files, layout));
    files.kernel.size = 64;
    files.dtb.size = UINT64_MAX;
    EXPECT_FALSE(guest::CalculateGuestLayout(files, layout));
    files.dtb.size = 64;
    files.initrd.size = UINT64_MAX;
    EXPECT_FALSE(guest::CalculateGuestLayout(files, layout));
}

TEST(Guest, RejectsEmptyDtbAndEmptyPresentInitrd) {
    for (bool emptyDtb : { false, true }) {
        std::vector<uint8_t> bytes;
        fixture::Entry(bytes, "linux/Image", { 1 });
        fixture::Entry(
                bytes, "linux/guest.dtb", emptyDtb ? std::vector<uint8_t> {} : buildGuestDtb());
        fixture::Entry(bytes, "linux/initrd");
        fixture::Finish(bytes);
        EXPECT_EQ(guest::LoadLinuxGuest(cpio::Archive(bytes.data(), bytes.size())).error,
                emptyDtb ? guest::LoadError::MISSING_DTB : guest::LoadError::EMPTY_INITRD);
    }
}

TEST(Guest, CopiesFilesAndPatchesDtb) {
    for (bool withInitrd : { false, true }) {
        std::vector<uint8_t> ram(guest::GUEST_RAM_SIZE);
        gAllocPagesReturn = reinterpret_cast<uint64_t>(ram.data());
        auto bytes { buildArchive(withInitrd) };
        auto original { bytes };
        cpio::Archive archive { bytes.data(), bytes.size() };
        guest::LinuxFiles files {};
        ASSERT_EQ(guest::ReadLinuxFiles(archive, files), guest::LoadError::NONE);
        guest::GuestLayout layout {};
        ASSERT_TRUE(guest::CalculateGuestLayout(files, layout));
        auto loaded { guest::LoadLinuxGuest(archive) };
        ASSERT_TRUE(loaded.isLoaded);
        EXPECT_EQ(gAllocPagesOrder, 16);
        EXPECT_EQ(loaded.guest.entryIpa, layout.entryIpa);
        EXPECT_EQ(loaded.guest.dtbIpa, layout.dtbIpa);
        EXPECT_TRUE(std::equal(files.kernel.data,
                files.kernel.data + files.kernel.size,
                ram.data() + layout.kernelIpa - guest::GUEST_IPA_BASE));
        if (withInitrd)
            EXPECT_TRUE(std::equal(files.initrd.data,
                    files.initrd.data + files.initrd.size,
                    ram.data() + layout.initrdIpa - guest::GUEST_IPA_BASE));
        std::vector<uint8_t> dtb(
                ram.begin() + layout.dtbIpa - guest::GUEST_IPA_BASE,
                ram.begin() + layout.dtbIpa - guest::GUEST_IPA_BASE + layout.dtbSize);
        EXPECT_EQ(readBe64Cells(dtb, findPropData(dtb, "reg")), guest::GUEST_IPA_BASE);
        EXPECT_EQ(readBe64Cells(dtb, findPropData(dtb, "reg") + 8), guest::GUEST_RAM_SIZE);
        EXPECT_EQ(readBe64Cells(dtb, findPropData(dtb, "linux,initrd-start")), layout.initrdIpa);
        EXPECT_EQ(readBe64Cells(dtb, findPropData(dtb, "linux,initrd-end")),
                layout.initrdIpa + layout.initrdSize);
        EXPECT_EQ(bytes, original);
    }
}

TEST(Guest, ReleasesRamOnInvalidDtb) {
    std::vector<uint8_t> ram(guest::GUEST_RAM_SIZE);
    gAllocPagesReturn = reinterpret_cast<uint64_t>(ram.data());
    auto bytes { buildArchive(true, false) };
    freed = 0;
    EXPECT_EQ(guest::LoadLinuxGuest(cpio::Archive(bytes.data(), bytes.size())).error,
            guest::LoadError::GUEST_DTB_PATCH_FAILED);
    EXPECT_EQ(freed, 1);
}

TEST(Guest, RejectsMalformedDtbBoundsAndTokens) {
    std::vector<uint8_t> ram(guest::GUEST_RAM_SIZE);
    gAllocPagesReturn = reinterpret_cast<uint64_t>(ram.data());
    for (unsigned offset : { 4U, 8U, 12U, 32U, 36U, 56U }) {
        auto bytes { buildArchive() };
        cpio::File dtb {};
        ASSERT_TRUE(cpio::Archive(bytes.data(), bytes.size()).Find("linux/guest.dtb", dtb));
        size_t start { static_cast<size_t>(dtb.data - bytes.data()) };
        for (unsigned i {}; i < 4; ++i)
            bytes[start + offset + i] = 0xff;
        freed = 0;
        EXPECT_EQ(guest::LoadLinuxGuest(cpio::Archive(bytes.data(), bytes.size())).error,
                guest::LoadError::GUEST_DTB_PATCH_FAILED);
        EXPECT_EQ(freed, 1);
    }
}

TEST(Guest, ReportsInvalidArchiveAndAllocationFailure) {
    EXPECT_EQ(guest::LoadLinuxGuest(cpio::Archive(nullptr, 0)).error,
            guest::LoadError::INVALID_ARCHIVE);
    auto bytes { buildArchive() };
    gAllocPagesReturn = 0;
    EXPECT_EQ(guest::LoadLinuxGuest(cpio::Archive(bytes.data(), bytes.size())).error,
            guest::LoadError::GUEST_RAM_ALLOCATION_FAILED);
}
