// @file bootLoader.cpp
// @brief Loads the Linux guest from the firmware archive into guest RAM.
// @ingroup core

#include "bootLoader.h"
#include "core/deviceTree/fdt.h"
#include "core/mm/mmu/hostMmu/hostMmu.h"
#include "core/mm/pageTable/pageTable.h"
#include "core/mm/pmm/pmm.h"
#include "lib/strings/strings.h"

#include <array>
#include <bit>
#include <memory>
#include <cstddef>

namespace {

constexpr uint64_t ALIGN_64K { 64ULL * 1024 };
constexpr uint64_t ALIGN_2MB { 2ULL * 1024 * 1024 };

// Smallest buddy order that covers GUEST_RAM_SIZE.
constexpr uint32_t GUEST_RAM_ORDER { static_cast<uint32_t>(
        std::bit_width((GUEST_RAM_SIZE >> PAGE_SHIFT) - 1)) };

constexpr uint64_t align4(uint64_t value) {
    return (value + 3) & ~static_cast<uint64_t>(3);
}

uint32_t loadBe32(const uint8_t* src) {
    uint32_t cell {};
    memcpy(&cell, src, sizeof(cell));
    return Be32(cell);
}

// Property data is only 4 byte aligned, so the value goes out through memcpy.
void storeBe64(uint8_t* dst, uint64_t value) {
    uint64_t be { Be64(value) };
    memcpy(dst, &be, sizeof(be));
}

struct GuestRamDeleter {
    void operator()(uint8_t* guestRam) const noexcept {
        if (guestRam)
            Pmm::GetInstance().FreePages(reinterpret_cast<uint64_t>(guestRam), GUEST_RAM_ORDER);
    }
};

// Walks the copied guest tree and fills the /memory/reg and /chosen initrd
// placeholders. A malformed tree, or one missing a placeholder, is rejected.
class DtbPatcher {
private:
    enum class Node : uint8_t {
        OTHER,
        MEMORY, // a /memory node
        CHOSEN, // /chosen
    };

    struct Blocks {
        uint64_t cursor;    // offset of the next structure block token
        uint64_t structEnd;
        uint64_t stringsOff;
        uint64_t stringsSize;
    };

    struct Placeholder {
        Node node;
        const char* name;
        uint64_t length; // one big endian 64 bit value per 8 bytes
        std::array<uint64_t, 2> values;
        bool found;
    };

    uint8_t* m_base;
    uint64_t m_dtbSize;
    Blocks m_blocks {};
    uint32_t m_depth {};
    bool m_hasRoot {};
    Node m_node { Node::OTHER };
    std::array<Placeholder, 3> m_placeholders;

    bool readHeader() {
        if (m_dtbSize < sizeof(FdtHeader))
            return false;

        FdtHeader header {};
        memcpy(&header, m_base, sizeof(header));

        uint64_t total { Be32(header.totalSize) };
        uint64_t structOff { Be32(header.structOff) };
        uint64_t structSize { Be32(header.sizeStructs) };
        uint64_t stringsOff { Be32(header.stringsOff) };
        uint64_t stringsSize { Be32(header.sizeStrings) };

        auto inBounds = [total](uint64_t off, uint64_t size) {
            return off >= sizeof(FdtHeader) && off <= total && size <= total - off;
        };

        if (Be32(header.magic) != static_cast<uint32_t>(FDT::MAGIC) || total < sizeof(FdtHeader) ||
                total > m_dtbSize)
            return false;

        if (!inBounds(structOff, structSize) || (structOff & 3) != 0 ||
                !inBounds(stringsOff, stringsSize))
            return false;

        m_blocks = { structOff, structOff + structSize, stringsOff, stringsSize };

        return structOff >= stringsOff + stringsSize || stringsOff >= m_blocks.structEnd;
    }

    bool beginNode() {
        uint64_t nameOff { m_blocks.cursor };

        while (m_blocks.cursor < m_blocks.structEnd && m_base[m_blocks.cursor] != 0) {
            ++m_blocks.cursor;
        }

        if (m_blocks.cursor == m_blocks.structEnd)
            return false;

        const char* name { reinterpret_cast<const char*>(m_base + nameOff) };

        if (m_depth == 0) {
            if (m_hasRoot || *name != 0)
                return false;

            m_hasRoot = true;
        } else if (m_depth == 1) {
            if (StrEq(name, "memory") || StrStartsWith(name, "memory@"))
                m_node = Node::MEMORY;
            else if (StrEq(name, "chosen"))
                m_node = Node::CHOSEN;
            else
                m_node = Node::OTHER;
        }

        m_blocks.cursor = align4(m_blocks.cursor + 1);
        ++m_depth;
        return true;
    }

    bool property() {
        if (m_depth == 0 || m_blocks.structEnd - m_blocks.cursor < sizeof(FdtProp))
            return false;

        FdtProp prop {};
        memcpy(&prop, m_base + m_blocks.cursor, sizeof(prop));
        m_blocks.cursor += sizeof(prop);

        uint64_t length { Be32(prop.dataLen) };
        uint64_t nameOff { Be32(prop.nameOff) };

        if (length > m_blocks.structEnd - m_blocks.cursor || nameOff >= m_blocks.stringsSize)
            return false;

        uint64_t nameEnd { nameOff };

        while (nameEnd < m_blocks.stringsSize && m_base[m_blocks.stringsOff + nameEnd] != 0) {
            ++nameEnd;
        }

        if (nameEnd == m_blocks.stringsSize)
            return false;

        const char* name { reinterpret_cast<const char*>(m_base + m_blocks.stringsOff + nameOff) };
        uint8_t* value { m_base + m_blocks.cursor };
        m_blocks.cursor = align4(m_blocks.cursor + length);

        return m_depth != 2 || patch(name, value, length);
    }

    bool patch(const char* name, uint8_t* value, uint64_t length) {
        for (Placeholder& placeholder : m_placeholders) {
            if (placeholder.node != m_node || !StrEq(name, placeholder.name))
                continue;

            if (placeholder.found || length != placeholder.length)
                return false;

            for (uint64_t i {}; i < length / sizeof(uint64_t); ++i) {
                storeBe64(value + i * sizeof(uint64_t), placeholder.values[i]);
            }

            placeholder.found = true;
            break;
        }

        return true;
    }

    [[nodiscard]] bool finish() const {
        if (!m_hasRoot || m_depth != 0)
            return false;

        for (const Placeholder& placeholder : m_placeholders) {
            if (!placeholder.found)
                return false;
        }

        return true;
    }

public:
    DtbPatcher(void* dtb, const GuestLayout& layout) :
                m_base { static_cast<uint8_t*>(dtb) }, m_dtbSize { layout.dtbSize },
                m_placeholders { {
                        { Node::MEMORY, "reg", 16, { GUEST_IPA_BASE, GUEST_RAM_SIZE }, false },
                        { Node::CHOSEN, "linux,initrd-start", 8, { layout.initrdIpa }, false },
                        { Node::CHOSEN,
                                "linux,initrd-end",
                                8,
                                { layout.initrdIpa + layout.initrdSize },
                                false },
                } } {}

    bool Run() {
        if (!readHeader())
            return false;

        while (m_blocks.cursor <= m_blocks.structEnd && m_blocks.structEnd - m_blocks.cursor >= 4) {
            FDT token { static_cast<FDT>(loadBe32(m_base + m_blocks.cursor)) };
            m_blocks.cursor += 4;

            switch (token) {
                case FDT::BEGIN_NODE:
                    if (!beginNode())
                        return false;
                    break;
                case FDT::END_NODE:
                    if (m_depth == 0)
                        return false;

                    if (--m_depth == 1)
                        m_node = Node::OTHER;
                    break;
                case FDT::PROP:
                    if (!property())
                        return false;
                    break;
                case FDT::NOP:
                    break;
                case FDT::END:
                    return finish();
                default:
                    return false;
            }
        }

        return false;
    }
};

} // namespace

BootLoader::BootLoader(const cpio::Archive& archive) : m_archive { archive } {}

bool BootLoader::ReadFiles(GuestFiles& files) const {
    files = {};

    if (m_archive.GetError() != cpio::Error::NONE)
        return false;

    if (!m_archive.Find("linux/Image", files.kernel) || files.kernel.size == 0)
        return false;

    if (!m_archive.Find("linux/guest.dtb", files.dtb) || files.dtb.size == 0)
        return false;

    if (m_archive.Find("linux/initrd", files.initrd) && files.initrd.size == 0)
        return false;

    return true;
}

bool BootLoader::CalculateLayout(const GuestFiles& files, GuestLayout& out) {
    auto alignDown = [](uint64_t value, uint64_t alignment) { return value & ~(alignment - 1); };

    out = {};

    if (files.kernel.size == 0 || files.dtb.size == 0)
        return false;

    uint64_t kernelEnd {};

    if (__builtin_add_overflow(KERNEL_LOAD_IPA, files.kernel.size, &kernelEnd))
        return false;

    uint64_t top { GUEST_IPA_BASE + GUEST_RAM_SIZE };
    uint64_t initrdIpa {};

    if (files.initrd.size != 0) {
        if (files.initrd.size > top)
            return false;

        initrdIpa = alignDown(top - files.initrd.size, ALIGN_2MB);

        if (initrdIpa < kernelEnd)
            return false;

        top = initrdIpa;
    }

    if (files.dtb.size > top)
        return false;

    uint64_t dtbIpa { alignDown(top - files.dtb.size, ALIGN_64K) };

    if (dtbIpa < kernelEnd)
        return false;

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

    layout.ramHostPa = Pmm::GetInstance().AllocPages(GUEST_RAM_ORDER);

    if (layout.ramHostPa == 0)
        return false;

    std::unique_ptr<uint8_t, GuestRamDeleter> guestRam { reinterpret_cast<uint8_t*>(
            layout.ramHostPa) };

    auto copyToGuest = [&layout](uint64_t ipa, const cpio::File& file) {
        void* dest { HostMmu::PaToVa(layout.IpaToHostPa(ipa)) };
        memcpy(dest, file.data, static_cast<size_t>(file.size));
        PageTable::CleanDataCacheRange(dest, static_cast<size_t>(file.size));
    };

    copyToGuest(layout.kernelIpa, files.kernel);
    copyToGuest(layout.dtbIpa, files.dtb);

    void* guestDtb { HostMmu::PaToVa(layout.IpaToHostPa(layout.dtbIpa)) };

    if (!DtbPatcher { guestDtb, layout }.Run())
        return false;

    PageTable::CleanDataCacheRange(guestDtb, static_cast<size_t>(layout.dtbSize));

    if (files.initrd.size != 0)
        copyToGuest(layout.initrdIpa, files.initrd);

    (void)guestRam.release();
    out = layout;
    return true;
}
