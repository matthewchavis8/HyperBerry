#include "bootLoader.h"

#include "core/deviceTree/fdt.h"
#include "core/mm/mmu/hostMmu/hostMmu.h"
#include "core/mm/pageTable/pageTable.h"
#include "core/mm/pmm/pmm.h"
#include "lib/log/log.h"
#include "lib/strings/strings.h"

#include <bit>
#include <memory>
#include <stddef.h>

namespace {

constexpr uint64_t ALIGN_64K { 64ULL * 1024 };
constexpr uint64_t ALIGN_2MB { 2ULL * 1024 * 1024 };

// Smallest buddy order that covers GUEST_RAM_SIZE.
constexpr uint32_t GUEST_RAM_ORDER { static_cast<uint32_t>(
        std::bit_width((GUEST_RAM_SIZE >> PAGE_SHIFT) - 1)) };

bool fail(const char* reason) {
    Log::Println("[BootLoader] {}", reason);
    return false;
}

constexpr uint64_t align4(uint64_t value) {
    return (value + 3) & ~static_cast<uint64_t>(3);
}

uint32_t loadBe32(const uint8_t* src) {
    uint32_t cell {};
    memcpy(&cell, src, sizeof(cell));
    return Be32(cell);
}

// Property data is only 4 byte aligned, so the two cells go out through memcpy.
void storeBe64Cells(uint8_t* dst, uint64_t value) {
    uint32_t cells[2] {
        Be32(static_cast<uint32_t>(value >> 32)),
        Be32(static_cast<uint32_t>(value)),
    };
    memcpy(dst, cells, sizeof(cells));
}

struct GuestRamDeleter {
    void operator()(uint8_t* guestRam) const noexcept {
        if (guestRam)
            pmm::FreePages(reinterpret_cast<uint64_t>(guestRam), GUEST_RAM_ORDER);
    }
};

void copyToGuest(const GuestLayout& layout, uint64_t ipa, const cpio::File& file) {
    void* dest { HostMmu::PaToVa(layout.IpaToHostPa(ipa)) };
    memcpy(dest, file.data, static_cast<size_t>(file.size));
    PageTable::CleanDataCacheRange(dest, static_cast<size_t>(file.size));
}

// Walks the copied guest tree and fills the /memory/reg and /chosen initrd
// placeholders. A malformed tree, or one missing a placeholder, is rejected.
class DtbPatcher {
private:
    uint8_t* m_base;
    GuestLayout m_layout;
    uint64_t m_cursor {}; // offset of the next structure block token
    uint64_t m_structEnd {};
    uint64_t m_stringsOff {};
    uint64_t m_stringsSize {};
    uint32_t m_depth {};
    bool m_hasRoot {};
    bool m_inMemory {}; // inside a /memory node
    bool m_inChosen {}; // inside /chosen
    bool m_hasMemoryReg {};
    bool m_hasInitrdStart {};
    bool m_hasInitrdEnd {};

    bool readHeader() {
        if (m_layout.dtbSize < sizeof(FdtHeader))
            return fail("guest DTB is smaller than its header");

        FdtHeader header {};
        memcpy(&header, m_base, sizeof(header));

        uint64_t total { Be32(header.totalSize) };
        uint64_t structOff { Be32(header.structOff) };
        uint64_t structSize { Be32(header.sizeStructs) };
        m_stringsOff = Be32(header.stringsOff);
        m_stringsSize = Be32(header.sizeStrings);

        auto inBounds = [total](uint64_t off, uint64_t size) {
            return off >= sizeof(FdtHeader) && off <= total && size <= total - off;
        };

        if (Be32(header.magic) != static_cast<uint32_t>(FDT::MAGIC) || total < sizeof(FdtHeader) ||
                total > m_layout.dtbSize)
            return fail("guest DTB header is invalid");

        if (!inBounds(structOff, structSize) || (structOff & 3) != 0 ||
                !inBounds(m_stringsOff, m_stringsSize))
            return fail("guest DTB blocks fall outside the tree");

        m_cursor = structOff;
        m_structEnd = structOff + structSize;

        if (m_cursor < m_stringsOff + m_stringsSize && m_stringsOff < m_structEnd)
            return fail("guest DTB structure and strings blocks overlap");

        return true;
    }

    bool beginNode() {
        uint64_t nameOff { m_cursor };

        while (m_cursor < m_structEnd && m_base[m_cursor] != 0) {
            ++m_cursor;
        }

        if (m_cursor == m_structEnd)
            return fail("guest DTB node name is unterminated");

        const char* name { reinterpret_cast<const char*>(m_base + nameOff) };

        if (m_depth == 0) {
            if (m_hasRoot || *name != 0)
                return fail("guest DTB root node is malformed");

            m_hasRoot = true;
        } else if (m_depth == 1) {
            m_inMemory = StrEq(name, "memory") || StrStartsWith(name, "memory@");
            m_inChosen = StrEq(name, "chosen");
        }

        m_cursor = align4(m_cursor + 1);
        ++m_depth;
        return true;
    }

    bool endNode() {
        if (m_depth == 0)
            return fail("guest DTB has an unmatched end node");

        --m_depth;

        if (m_depth == 1) {
            m_inMemory = false;
            m_inChosen = false;
        }

        return true;
    }

    bool property() {
        if (m_depth == 0 || m_structEnd - m_cursor < sizeof(FdtProp))
            return fail("guest DTB property is truncated");

        FdtProp prop {};
        memcpy(&prop, m_base + m_cursor, sizeof(prop));
        m_cursor += sizeof(prop);

        uint64_t length { Be32(prop.dataLen) };
        uint64_t nameOff { Be32(prop.nameOff) };

        if (length > m_structEnd - m_cursor || nameOff >= m_stringsSize)
            return fail("guest DTB property is out of bounds");

        uint64_t nameEnd { nameOff };

        while (nameEnd < m_stringsSize && m_base[m_stringsOff + nameEnd] != 0) {
            ++nameEnd;
        }

        if (nameEnd == m_stringsSize)
            return fail("guest DTB property name is unterminated");

        const char* name { reinterpret_cast<const char*>(m_base + m_stringsOff + nameOff) };
        uint8_t* value { m_base + m_cursor };
        m_cursor = align4(m_cursor + length);

        return m_depth != 2 || patch(name, value, length);
    }

    bool patch(const char* name, uint8_t* value, uint64_t length) {
        if (m_inMemory && StrEq(name, "reg")) {
            if (m_hasMemoryReg || length != 16)
                return fail("guest DTB /memory/reg is repeated or not 16 bytes");

            storeBe64Cells(value, GUEST_IPA_BASE);
            storeBe64Cells(value + 8, GUEST_RAM_SIZE);
            m_hasMemoryReg = true;
        } else if (m_inChosen && StrEq(name, "linux,initrd-start")) {
            if (m_hasInitrdStart || length != 8)
                return fail("guest DTB linux,initrd-start is repeated or not 8 bytes");

            storeBe64Cells(value, m_layout.initrdIpa);
            m_hasInitrdStart = true;
        } else if (m_inChosen && StrEq(name, "linux,initrd-end")) {
            if (m_hasInitrdEnd || length != 8)
                return fail("guest DTB linux,initrd-end is repeated or not 8 bytes");

            storeBe64Cells(value, m_layout.initrdIpa + m_layout.initrdSize);
            m_hasInitrdEnd = true;
        }

        return true;
    }

    [[nodiscard]] bool finish() const {
        if (!m_hasRoot || m_depth != 0)
            return fail("guest DTB ends inside a node");

        if (!m_hasMemoryReg || !m_hasInitrdStart || !m_hasInitrdEnd)
            return fail("guest DTB lacks the /memory/reg or /chosen initrd placeholders");

        return true;
    }

public:
    DtbPatcher(void* dtb, const GuestLayout& layout) :
                m_base { static_cast<uint8_t*>(dtb) }, m_layout { layout } {}

    bool Run() {
        if (!readHeader())
            return false;

        while (m_cursor <= m_structEnd && m_structEnd - m_cursor >= 4) {
            FDT token { static_cast<FDT>(loadBe32(m_base + m_cursor)) };
            m_cursor += 4;

            bool isValid { true };

            switch (token) {
                case FDT::BEGIN_NODE:
                    isValid = beginNode();
                    break;
                case FDT::END_NODE:
                    isValid = endNode();
                    break;
                case FDT::PROP:
                    isValid = property();
                    break;
                case FDT::NOP:
                    break;
                case FDT::END:
                    return finish();
                default:
                    return fail("guest DTB has an unknown token");
            }

            if (!isValid)
                return false;
        }

        return fail("guest DTB structure block has no end token");
    }
};
} // namespace

BootLoader::BootLoader(const cpio::Archive& archive) : m_archive { archive } {}

bool BootLoader::ReadFiles(GuestFiles& files) const {
    files = {};

    if (m_archive.GetError() != cpio::Error::NONE)
        return fail("archive is invalid");

    if (!m_archive.Find("linux/Image", files.kernel) || files.kernel.size == 0)
        return fail("linux/Image is missing or empty");

    if (!m_archive.Find("linux/guest.dtb", files.dtb) || files.dtb.size == 0)
        return fail("linux/guest.dtb is missing or empty");

    if (m_archive.Find("linux/initrd", files.initrd) && files.initrd.size == 0)
        return fail("linux/initrd is empty");

    return true;
}

bool BootLoader::CalculateLayout(const GuestFiles& files, GuestLayout& out) {
    auto alignDown = [](uint64_t value, uint64_t alignment) { return value & ~(alignment - 1); };

    out = {};

    if (files.kernel.size == 0 || files.dtb.size == 0)
        return fail("kernel or guest DTB is empty");

    uint64_t kernelEnd {};

    if (__builtin_add_overflow(KERNEL_LOAD_IPA, files.kernel.size, &kernelEnd))
        return fail("kernel size overflows the guest IPA space");

    uint64_t top { GUEST_IPA_BASE + GUEST_RAM_SIZE };
    uint64_t initrdIpa {};

    if (files.initrd.size != 0) {
        if (files.initrd.size > top)
            return fail("initrd does not fit in guest RAM");

        initrdIpa = alignDown(top - files.initrd.size, ALIGN_2MB);

        if (initrdIpa < kernelEnd)
            return fail("initrd does not fit above the kernel");

        top = initrdIpa;
    }

    if (files.dtb.size > top)
        return fail("guest DTB does not fit in guest RAM");

    uint64_t dtbIpa { alignDown(top - files.dtb.size, ALIGN_64K) };

    if (dtbIpa < kernelEnd)
        return fail("guest DTB does not fit above the kernel");

    out.kernelIpa = KERNEL_LOAD_IPA;
    out.kernelSize = files.kernel.size;
    out.dtbIpa = dtbIpa;
    out.dtbSize = files.dtb.size;
    out.initrdIpa = initrdIpa;
    out.initrdSize = files.initrd.size;
    return true;
}

bool BootLoader::Load(GuestLayout& out) const {
    out = {};

    GuestFiles files {};
    GuestLayout layout {};

    if (!ReadFiles(files) || !CalculateLayout(files, layout))
        return false;

    layout.ramHostPa = pmm::AllocPages(GUEST_RAM_ORDER);

    if (layout.ramHostPa == 0)
        return fail("cannot allocate guest RAM");

    std::unique_ptr<uint8_t, GuestRamDeleter> guestRam { reinterpret_cast<uint8_t*>(
            layout.ramHostPa) };

    copyToGuest(layout, layout.kernelIpa, files.kernel);
    copyToGuest(layout, layout.dtbIpa, files.dtb);

    void* guestDtb { HostMmu::PaToVa(layout.IpaToHostPa(layout.dtbIpa)) };

    if (!DtbPatcher { guestDtb, layout }.Run())
        return false;

    PageTable::CleanDataCacheRange(guestDtb, static_cast<size_t>(layout.dtbSize));

    if (files.initrd.size != 0)
        copyToGuest(layout, layout.initrdIpa, files.initrd);

    (void)guestRam.release();
    out = layout;
    return true;
}
