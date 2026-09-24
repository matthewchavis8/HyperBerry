#ifndef __BINARY_H__
#define __BINARY_H__

#include "lib/cpio/cpio.h"
#include "lib/strings/strings.h"
#include "core/mm/pmm/pmm.h"
#include "core/mm/mmu/hostMmu/hostMmu.h"
#include "tests/integration/suite.h"

namespace test {
class Binary {
    uint64_t m_base {};
    uint32_t m_order {};

public:
    explicit Binary(const char* path) {
        const auto& map { TestRunner::BootMemoryMap() };
        cpio::Archive archive { HostMmu::PaToVa(map.cpioArchiveBase), map.cpioArchiveSize };
        cpio::File file {};
        if (!archive.Find(path, file) || !file.size) return;
        while (m_order < MAX_ORDER && (PAGE_SIZE << m_order) < file.size)
            ++m_order;
        if ((PAGE_SIZE << m_order) < file.size) return;
        m_base = pmm::AllocPages(m_order);
        if (!m_base) return;
        auto* destination { static_cast<uint8_t*>(HostMmu::PaToVa(m_base)) };
        memcpy(destination, file.data, file.size);
        uint64_t ctr {};
        asm volatile("mrs %0, ctr_el0" : "=r"(ctr));
        uint64_t dataLine { 4ULL << ((ctr >> 16) & 15) };
        uint64_t instructionLine { 4ULL << (ctr & 15) };
        uintptr_t start { reinterpret_cast<uintptr_t>(destination) };
        for (uintptr_t p { start & ~(dataLine - 1) }; p < start + file.size; p += dataLine)
            asm volatile("dc cvau, %0" ::"r"(p) : "memory");
        asm volatile("dsb ish" ::: "memory");
        for (uintptr_t p { start & ~(instructionLine - 1) }; p < start + file.size;
                p += instructionLine)
            asm volatile("ic ivau, %0" ::"r"(p) : "memory");
        asm volatile("dsb ish\nisb" ::: "memory");
    }

    uint64_t GetEntry() const { return m_base; }

    Binary(const Binary&) = delete;
    Binary& operator=(const Binary&) = delete;
    Binary(Binary&&) = delete;
    Binary& operator=(Binary&&) = delete;
    ~Binary() {
        if (m_base) pmm::FreePages(m_base, m_order);
    }
};
} // namespace test

#endif // !__BINARY_H__
