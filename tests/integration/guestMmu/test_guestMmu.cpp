// @file test_guestMmu.cpp
// @brief Integration tests for live EL2 stage-2 MMU state.

#include "regs.inc"
#include "core/mm/mmu/guestMmu/guestMmu.h"
#include "core/mm/pmm/pmm.h"
#include "tests/integration/suite.h"

namespace {
constexpr uint64_t kGuestIpaBase { 0x40000000ULL };
constexpr uint64_t kGuestSize { SIZE_2MB * 2 };
constexpr uint8_t kTestVmid { 7 };
constexpr uint64_t kDeviceBlockIpa { 0x50000000ULL };
constexpr uint64_t kDevicePageIpa { 0x51000000ULL };

struct GuestMmuLayout {
    uint64_t* rootTable;
    uint8_t vmid;
};

uint64_t* rootTable(GuestMmu& mmu) {
    return reinterpret_cast<GuestMmuLayout*>(&mmu)->rootTable;
}

uint64_t* walkStage2L2(uint64_t* root, uint64_t ipa) {
    if (!root) return nullptr;

    uint64_t l1Entry { root[(ipa >> 30) & 0x3FFULL] };
    if (!pte_is_table(l1Entry)) return nullptr;

    uint64_t* l2 { pte_next_table(l1Entry) };
    return &l2[L2_INDEX(ipa)];
}

uint64_t* walkStage2L3(uint64_t* root, uint64_t ipa) {
    uint64_t* l2 { walkStage2L2(root, ipa) };
    if (!l2 || !pte_is_table(*l2)) return nullptr;

    uint64_t* l3 { pte_next_table(*l2) };
    return &l3[L3_INDEX(ipa)];
}

uint64_t normalStage2Descriptor(uint64_t pa) {
    return (pa & PTE_ADDR_MASK) | PTE_VALID | PTE_BLOCK | PTE_AF | S2PTE_SH_INNER | S2PTE_S2AP_RW |
            S2PTE_MEMATTR_NORMAL_WB | S2PTE_XN_NONE;
}

uint64_t deviceStage2Descriptor(uint64_t pa) {
    return (pa & PTE_ADDR_MASK) | PTE_VALID | PTE_BLOCK | PTE_AF | S2PTE_SH_INNER | S2PTE_S2AP_RW |
            S2PTE_MEMATTR_DEVICE_nGnRnE | S2PTE_XN_ALL;
}

uint64_t devicePageStage2Descriptor(uint64_t pa) {
    return deviceStage2Descriptor(pa) | PTE_TABLE;
}

void clearStage2Enable() {
    uint64_t hcr;
    asm volatile("mrs %0, hcr_el2" : "=r"(hcr));
    hcr &= ~1ULL;
    asm volatile("msr hcr_el2, %0\n"
                 "isb" ::"r"(hcr)
            : "memory");
}
} // namespace

static bool test_init_programs_vtcr_el2() {
    GuestMmu mmu;
    uint64_t hostPa { Pmm::GetInstance().AllocPages(9) };
    if (hostPa == 0) return false;

    mmu.Init(kGuestIpaBase, hostPa, kGuestSize, MmioMap {});

    uint64_t vtcr;
    asm volatile("mrs %0, vtcr_el2" : "=r"(vtcr));
    Pmm::GetInstance().FreePages(hostPa, 9);

    uint64_t expected { VTCR_T0SZ(24) | VTCR_SL0_L1 | VTCR_TG0_4K | VTCR_SH0_IS | VTCR_ORGN0_WB |
        VTCR_IRGN0_WB | VTCR_PS_40BIT | VTCR_RES1 };

    return vtcr == expected;
}

static bool test_init_maps_guest_ram_blocks() {
    GuestMmu mmu;
    uint64_t hostPa { Pmm::GetInstance().AllocPages(9) };
    if (hostPa == 0) return false;

    mmu.Init(kGuestIpaBase, hostPa, kGuestSize, MmioMap {});
    uint64_t* first { walkStage2L2(rootTable(mmu), kGuestIpaBase) };
    uint64_t* second { walkStage2L2(rootTable(mmu), kGuestIpaBase + SIZE_2MB) };

    bool mapped { first != nullptr && second != nullptr &&
        *first == normalStage2Descriptor(hostPa) &&
        *second == normalStage2Descriptor(hostPa + SIZE_2MB) };

    Pmm::GetInstance().FreePages(hostPa, 9);
    return mapped;
}

static bool test_init_maps_device_windows() {
    GuestMmu mmu;
    uint64_t hostPa { Pmm::GetInstance().AllocPages(9) };
    if (hostPa == 0) return false;

    // One window of each granule. The page path carried no coverage at all
    // while the windows were a fixed table in a board header.
    MmioMap devices {};
    devices.AddBlocks(kDeviceBlockIpa, SIZE_2MB);
    devices.AddPages(kDevicePageIpa, BSP_UART_BASE, SIZE_4KB);

    mmu.Init(kGuestIpaBase, hostPa, kGuestSize, devices);

    uint64_t* block { walkStage2L2(rootTable(mmu), kDeviceBlockIpa) };
    uint64_t* page { walkStage2L3(rootTable(mmu), kDevicePageIpa) };

    bool mapped { block != nullptr && *block == deviceStage2Descriptor(kDeviceBlockIpa) &&
        page != nullptr && *page == devicePageStage2Descriptor(BSP_UART_BASE) };

    Pmm::GetInstance().FreePages(hostPa, 9);
    return mapped;
}

static bool test_enable_programs_vttbr_and_hcr_vm() {
    GuestMmu mmu;
    uint64_t hostPa { Pmm::GetInstance().AllocPages(9) };
    if (hostPa == 0) return false;

    mmu.Init(kGuestIpaBase, hostPa, kGuestSize, MmioMap {});
    mmu.Enable(kTestVmid);

    uint64_t vttbr;
    uint64_t hcr;
    asm volatile("mrs %0, vttbr_el2" : "=r"(vttbr));
    asm volatile("mrs %0, hcr_el2" : "=r"(hcr));

    uint64_t expectedVttbr { (static_cast<uint64_t>(kTestVmid) << 48) |
        (reinterpret_cast<uint64_t>(rootTable(mmu)) & PTE_ADDR_MASK) };
    bool enabled { vttbr == expectedVttbr && (hcr & 1ULL) != 0 };

    clearStage2Enable();
    Pmm::GetInstance().FreePages(hostPa, 9);
    return enabled;
}

static bool test_device_pages_replace_ram_without_losing_neighbors() {
    GuestMmu mmu;
    uint64_t hostPa { Pmm::GetInstance().AllocPages(10) };
    if (!hostPa) return false;
    uint64_t deviceIpa { kGuestIpaBase + SIZE_4KB };
    MmioMap devices {};
    devices.AddPages(deviceIpa, BSP_UART_BASE, SIZE_4KB);
    mmu.Init(kGuestIpaBase, hostPa, kGuestSize, devices);
    uint64_t* first { walkStage2L3(rootTable(mmu), kGuestIpaBase) };
    uint64_t* device { walkStage2L3(rootTable(mmu), deviceIpa) };
    uint64_t* after { walkStage2L3(rootTable(mmu), deviceIpa + SIZE_4KB) };
    uint64_t* last { walkStage2L3(rootTable(mmu), kGuestIpaBase + SIZE_2MB - SIZE_4KB) };
    uint64_t* nextBlock { walkStage2L2(rootTable(mmu), kGuestIpaBase + SIZE_2MB) };
    bool mapped { first && device && after && last && nextBlock &&
        *first == (normalStage2Descriptor(hostPa) | PTE_TABLE) &&
        *device == devicePageStage2Descriptor(BSP_UART_BASE) &&
        *after == (normalStage2Descriptor(hostPa + 2 * SIZE_4KB) | PTE_TABLE) &&
        *last == (normalStage2Descriptor(hostPa + SIZE_2MB - SIZE_4KB) | PTE_TABLE) &&
        *nextBlock == normalStage2Descriptor(hostPa + SIZE_2MB) };
    Pmm::GetInstance().FreePages(hostPa, 10);
    return mapped;
}

static const TestCase kGuestMmuCases[] {
    { "init_programs_vtcr_el2", test_init_programs_vtcr_el2 },
    { "init_maps_guest_ram_blocks", test_init_maps_guest_ram_blocks },
    { "init_maps_device_windows", test_init_maps_device_windows },
    { "device_pages_replace_ram_without_losing_neighbors",
            test_device_pages_replace_ram_without_losing_neighbors },
    { "enable_programs_vttbr_and_hcr_vm", test_enable_programs_vttbr_and_hcr_vm },
};

static const TestSuite kGuestMmuSuite {
    "GuestMmuHarness",
    kGuestMmuCases,
    5,
};

REGISTER_SUITE(kGuestMmuSuite);
