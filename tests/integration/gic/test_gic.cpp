// @file test_gic.cpp
// @brief Hardware-backed integration tests for the GICv2 driver.

#include "core/vmm/esr.h"
#include "core/vcpu/vcpu.h"
#include "drivers/gic/gic.h"
#include "lib/log/log.h"
#include "tests/integration/suite.h"
#include "tests/integration/guest/binary.h"
#include <cstdint>

extern "C" {
volatile uint64_t gTestGicVectorExitKind { 0 };
}

namespace {
constexpr uint32_t kPhysIrq { 64 };
constexpr uint32_t kVirtIrq { 64 };
constexpr uint32_t kSpuriousIrq { 1023 };

namespace GicReg {
    namespace Dist {
        constexpr uintptr_t ISPENDR { 0x200 };
        constexpr uintptr_t ICPENDR { 0x280 };
        constexpr uintptr_t ISACTIVER { 0x300 };
        constexpr uintptr_t ICACTIVER { 0x380 };
        constexpr uintptr_t ITARGETSR { 0x800 };
        constexpr uintptr_t IGROUPR { 0x080 };
        constexpr uintptr_t CTLR { 0x000 };
    } // namespace Dist

    namespace Hv {
        constexpr uintptr_t VTR { 0x004 };
        constexpr uintptr_t ELSR0 { 0x030 };
        constexpr uintptr_t ELSR1 { 0x034 };
    } // namespace Hv
} // namespace GicReg

struct El2IrqCapture {
    bool called;
    uint32_t ackId;
    int injectRc;
};

struct EndToEndCapture {
    bool el2IrqSeen;
    bool el2AckedSpi;
    bool injected;
    bool pendingBeforeGuest;
    bool lrBusyBeforeGuest;
    bool guestExited;
    bool guestSawVirtIrq;
    bool physicalInactiveAfterEoi;
    bool pendingAfterGuest;
    bool lrFreeAfterGuest;
};

struct GuestResult {
    volatile uint32_t progress;
    volatile uint32_t iar;
};
static_assert(sizeof(GuestResult) == 8);
GuestResult gGuestResult {};

alignas(16) uint8_t gGuestStack[512];
El2IrqCapture gEl2Irq;

extern "C" char test_gic_vectors[];

void trace(const char* msg) {
    Log::Println("[GICDBG] {}", msg);
}

volatile uint32_t* distReg(uintptr_t offset) {
    return reinterpret_cast<volatile uint32_t*>(Gic::GetDistBase() + offset);
}

volatile uint32_t* hvReg(uintptr_t offset) {
    return reinterpret_cast<volatile uint32_t*>(Gic::GetHvBase() + offset);
}

uint32_t irqBit(uint32_t id) {
    return 1U << (id % 32);
}

uintptr_t irqReg(uintptr_t base, uint32_t id) {
    return base + (id / 32) * sizeof(uint32_t);
}

uint32_t readElsrBit(uint32_t slot) {
    uint32_t elsr { *hvReg(slot < 32 ? GicReg::Hv::ELSR0 : GicReg::Hv::ELSR1) };
    return (elsr >> (slot % 32)) & 1U;
}

uint32_t numListRegisters() {
    return (*hvReg(GicReg::Hv::VTR) & 0x3F) + 1;
}

void clearTestSpi() {
    *distReg(irqReg(GicReg::Dist::ICPENDR, kPhysIrq)) = irqBit(kPhysIrq);
    *distReg(irqReg(GicReg::Dist::ICACTIVER, kPhysIrq)) = irqBit(kPhysIrq);
}

void pendTestSpi() {
    *distReg(irqReg(GicReg::Dist::ISPENDR, kPhysIrq)) = irqBit(kPhysIrq);
}

void routeTestSpiToCpu0() {
    volatile uint8_t* target { reinterpret_cast<volatile uint8_t*>(
            Gic::GetDistBase() + GicReg::Dist::ITARGETSR + kPhysIrq) };
    *target = 0x01;
}

void enableDistributorGroups() {
    *distReg(GicReg::Dist::CTLR) = *distReg(GicReg::Dist::CTLR) | 0x3U;
}

void configureTestSpiGroup0() {
    // QEMU enters this image in a security state where a software-pended Group 1
    // SPI stays behind HPPIR 1022. Use Group 0 so the physical leg reaches EL2;
    // the LR still uses the driver's HW-bit physical-deactivation path.
    *distReg(irqReg(GicReg::Dist::IGROUPR, kPhysIrq)) =
            *distReg(irqReg(GicReg::Dist::IGROUPR, kPhysIrq)) & ~irqBit(kPhysIrq);
}

uint64_t installTestVbar() {
    uint64_t saved { 0 };
    asm volatile("mrs %0, vbar_el2" : "=r"(saved));
    uint64_t testVbar { reinterpret_cast<uint64_t>(test_gic_vectors) };
    asm volatile("msr vbar_el2, %0\n"
                 "isb" ::"r"(testVbar)
            : "memory");
    return saved;
}

void restoreVbar(uint64_t saved) {
    asm volatile("msr vbar_el2, %0\n"
                 "isb" ::"r"(saved)
            : "memory");
}

uint64_t saveDaif() {
    uint64_t saved { 0 };
    asm volatile("mrs %0, daif" : "=r"(saved));
    return saved;
}

void restoreDaif(uint64_t saved) {
    asm volatile("msr daif, %0\n"
                 "isb" ::"r"(saved)
            : "memory");
}

void unmaskIrq() {
    asm volatile("msr daifclr, #2\n"
                 "isb" ::
                         : "memory");
}

uint64_t enableVirtualIrqRouting() {
    constexpr uint64_t HCR_RW { 1ULL << 31 };
    constexpr uint64_t HCR_IMO { 1ULL << 4 };

    uint64_t saved { 0 };
    asm volatile("mrs %0, hcr_el2" : "=r"(saved));

    uint64_t hcr { saved | HCR_RW | HCR_IMO };
    asm volatile("msr hcr_el2, %0\n"
                 "isb" ::"r"(hcr)
            : "memory");

    return saved;
}

void restoreHcr(uint64_t saved) {
    asm volatile("msr hcr_el2, %0\n"
                 "isb" ::"r"(saved)
            : "memory");
}

bool waitForEl2Irq() {
    trace("waitForEl2Irq: enter");
    for (uint32_t i { 0 }; i < 1000000; ++i) {
        if (gEl2Irq.called) {
            Log::Println("[GICDBG] waitForEl2Irq: seen ackId={} injectRc={}",
                    gEl2Irq.ackId,
                    gEl2Irq.injectRc);
            return true;
        }
        asm volatile("nop");
    }
    Log::Println("[GICDBG] waitForEl2Irq: timeout called={}", gEl2Irq.called);
    return gEl2Irq.called;
}

bool injectFromPhysicalSpi() {
    trace("injectFromPhysicalSpi: enter");
    gEl2Irq = {};

    trace("injectFromPhysicalSpi: installTestVbar");
    uint64_t savedVbar { installTestVbar() };
    trace("injectFromPhysicalSpi: saveDaif");
    uint64_t savedDaif { saveDaif() };

    trace("injectFromPhysicalSpi: pendTestSpi");
    pendTestSpi();
    trace("injectFromPhysicalSpi: unmaskIrq");
    unmaskIrq();
    bool seen { waitForEl2Irq() };

    trace("injectFromPhysicalSpi: restoreDaif");
    restoreDaif(savedDaif);
    trace("injectFromPhysicalSpi: restoreVbar");
    restoreVbar(savedVbar);

    Log::Println("[GICDBG] injectFromPhysicalSpi: exit seen={} ackId={} injectRc={}",
            seen,
            gEl2Irq.ackId,
            gEl2Irq.injectRc);
    return seen && gEl2Irq.ackId == kPhysIrq && gEl2Irq.injectRc == 0;
}

EndToEndCapture runEndToEnd() {
    trace("runEndToEnd: enter");
    EndToEndCapture capture {};
    test::Binary binary { "tests/gic.bin" };
    if (!binary.GetEntry()) return capture;

    trace("runEndToEnd: Gic::init");
    Gic::Init();
    trace("runEndToEnd: clearTestSpi");
    clearTestSpi();
    trace("runEndToEnd: configureTestSpiGroup0");
    configureTestSpiGroup0();
    trace("runEndToEnd: routeTestSpiToCpu0");
    routeTestSpiToCpu0();
    trace("runEndToEnd: enableDistributorGroups");
    enableDistributorGroups();
    trace("runEndToEnd: setPriorityLevel");
    Gic::SetPriorityLevel(kPhysIrq, 0x80);
    trace("runEndToEnd: enableIrq");
    Gic::EnableIrq(kPhysIrq);

    trace("runEndToEnd: enableVirtualIrqRouting");
    uint64_t savedHcr { enableVirtualIrqRouting() };

    trace("runEndToEnd: injectFromPhysicalSpi");
    capture.el2IrqSeen = injectFromPhysicalSpi();
    capture.el2AckedSpi = gEl2Irq.ackId == kPhysIrq;
    capture.injected = gEl2Irq.injectRc == 0;
    trace("runEndToEnd: hasPendingIrq before guest");
    capture.pendingBeforeGuest = Gic::HasPendingIrq();
    trace("runEndToEnd: readElsrBit before guest");
    capture.lrBusyBeforeGuest = readElsrBit(0) == 0;
    Log::Println("[GICDBG] runEndToEnd: preguest seen={} acked={} injected={} pending={} lrBusy={}",
            capture.el2IrqSeen,
            capture.el2AckedSpi,
            capture.injected,
            capture.pendingBeforeGuest,
            capture.lrBusyBeforeGuest);

    if (!capture.el2IrqSeen || !capture.el2AckedSpi || !capture.injected) {
        trace("runEndToEnd: early cleanup");
        Gic::DisableIrq(kPhysIrq);
        clearTestSpi();
        Gic::CpuReset();
        restoreHcr(savedHcr);
        trace("runEndToEnd: early exit");
        return capture;
    }

    trace("runEndToEnd: Vcpu");
    Vcpu vcpu { binary.GetEntry() };
    vcpu.SetGpReg(VCPU_GPREG_X0, Gic::GetVcpuBase());
    vcpu.SetGpReg(VCPU_GPREG_X1, reinterpret_cast<uint64_t>(&gGuestResult));
    trace("runEndToEnd: Vcpu::setGuestSp");
    vcpu.SetGuestSp(reinterpret_cast<uint64_t>(gGuestStack) + sizeof(gGuestStack));

    trace("runEndToEnd: installTestVbar for guest");
    uint64_t savedVbar { installTestVbar() };
    gTestGicVectorExitKind = 0;
    gGuestResult.progress = 0;
    gGuestResult.iar = 0;
    trace("runEndToEnd: vcpu_enter begin");
    vcpu_enter(&vcpu);
    trace("runEndToEnd: vcpu_enter returned");
    Log::Println("[GICDBG] runEndToEnd: vectorExitKind={} guestProgress={:x} guestIar={:x} "
                 "guestX0={:x} guestElr={:x}",
            gTestGicVectorExitKind,
            gGuestResult.progress,
            gGuestResult.iar,
            vcpu.GetGpReg(VCPU_GPREG_X0),
            vcpu.GetElr());
    restoreVbar(savedVbar);

    capture.guestExited = true;
    trace("runEndToEnd: read guest x0");
    capture.guestSawVirtIrq = (vcpu.GetGpReg(VCPU_GPREG_X0) & 0x3FFU) == kVirtIrq;
    trace("runEndToEnd: read physical active");
    capture.physicalInactiveAfterEoi =
            (*distReg(irqReg(GicReg::Dist::ISACTIVER, kPhysIrq)) & irqBit(kPhysIrq)) == 0;
    trace("runEndToEnd: hasPendingIrq after guest");
    capture.pendingAfterGuest = Gic::HasPendingIrq();
    trace("runEndToEnd: readElsrBit after guest");
    capture.lrFreeAfterGuest = readElsrBit(0) == 1;
    Log::Println("[GICDBG] runEndToEnd: postguest guestSaw={} inactive={} pending={} lrFree={}",
            capture.guestSawVirtIrq,
            capture.physicalInactiveAfterEoi,
            capture.pendingAfterGuest,
            capture.lrFreeAfterGuest);

    trace("runEndToEnd: final cleanup");
    Gic::DisableIrq(kPhysIrq);
    clearTestSpi();
    Gic::CpuReset();
    restoreHcr(savedHcr);

    trace("runEndToEnd: exit");
    return capture;
}
} // namespace

extern "C" void handle_test_gic_el2_irq(ExceptionContext* ctx) {
    (void)ctx;

    trace("handle_test_gic_el2_irq: enter");
    Gic::IrqAck ack { Gic::AckIrq() };
    gEl2Irq.called = true;
    gEl2Irq.ackId = ack.id;
    Log::Println("[GICDBG] handle_test_gic_el2_irq: ack id={} iar={:x}", ack.id, ack.iar);

    if (ack.id != kSpuriousIrq) {
        trace("handle_test_gic_el2_irq: injectIrq");
        gEl2Irq.injectRc = Gic::InjectIrq(kVirtIrq, kPhysIrq);
        Log::Println("[GICDBG] handle_test_gic_el2_irq: injectRc={}", gEl2Irq.injectRc);
        trace("handle_test_gic_el2_irq: endIrq");
        Gic::EndIrq(ack);
    } else {
        gEl2Irq.injectRc = -1;
    }
    trace("handle_test_gic_el2_irq: exit");
}

static bool test_spi_to_virq_guest_eoi() {
    trace("test_spi_to_virq_guest_eoi: enter");
    EndToEndCapture capture { runEndToEnd() };
    bool passed { capture.el2IrqSeen && capture.el2AckedSpi && capture.injected &&
        capture.guestExited && capture.guestSawVirtIrq && capture.physicalInactiveAfterEoi };
    Log::Println("[GICDBG] test_spi_to_virq_guest_eoi: result={}", passed);
    return passed;
}

static bool test_lr_pending_state_tracks_guest_eoi() {
    trace("test_lr_pending_state_tracks_guest_eoi: enter");
    EndToEndCapture capture { runEndToEnd() };
    bool passed { capture.pendingBeforeGuest && capture.lrBusyBeforeGuest &&
        !capture.pendingAfterGuest && capture.lrFreeAfterGuest };
    Log::Println("[GICDBG] test_lr_pending_state_tracks_guest_eoi: result={}", passed);
    return passed;
}

static bool test_lr_exhaustion_returns_error() {
    trace("test_lr_exhaustion_returns_error: enter");
    trace("test_lr_exhaustion_returns_error: Gic::init");
    Gic::Init();
    trace("test_lr_exhaustion_returns_error: numListRegisters");
    uint32_t count { numListRegisters() };
    Log::Println("[GICDBG] test_lr_exhaustion_returns_error: count={}", count);

    for (uint32_t i { 0 }; i < count; ++i) {
        Log::Println("[GICDBG] test_lr_exhaustion_returns_error: inject slot {}", i);
        if (Gic::InjectIrq(100 + i, 100 + i) != 0) {
            trace("test_lr_exhaustion_returns_error: inject failed cleanup");
            Gic::CpuReset();
            return false;
        }
    }

    trace("test_lr_exhaustion_returns_error: inject exhausted");
    bool exhausted { Gic::InjectIrq(200, 200) == -1 };
    Log::Println("[GICDBG] test_lr_exhaustion_returns_error: exhausted={}", exhausted);
    trace("test_lr_exhaustion_returns_error: cpuReset");
    Gic::CpuReset();
    return exhausted;
}

static bool test_duplicate_virt_id_rejected() {
    trace("test_duplicate_virt_id_rejected: enter");
    trace("test_duplicate_virt_id_rejected: Gic::init");
    Gic::Init();

    trace("test_duplicate_virt_id_rejected: first inject");
    bool rejected { Gic::InjectIrq(kVirtIrq, kPhysIrq) == 0 &&
        (trace("test_duplicate_virt_id_rejected: duplicate inject"), true) &&
        Gic::InjectIrq(kVirtIrq, 99) == -1 };
    Log::Println("[GICDBG] test_duplicate_virt_id_rejected: rejected={}", rejected);

    trace("test_duplicate_virt_id_rejected: cpuReset");
    Gic::CpuReset();
    return rejected;
}

static const TestCase kGicCases[] {
    { "spi_to_virq_guest_eoi", test_spi_to_virq_guest_eoi },
    { "lr_pending_state_tracks_eoi", test_lr_pending_state_tracks_guest_eoi },
    { "lr_exhaustion_returns_error", test_lr_exhaustion_returns_error },
    { "duplicate_virt_id_rejected", test_duplicate_virt_id_rejected },
};

static const TestSuite kGicSuite {
    "GicHarness",
    kGicCases,
    4,
};

REGISTER_SUITE(kGicSuite);
