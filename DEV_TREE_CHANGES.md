# What changed on dev: d087f07..9f563c5

> Two more rounds follow.
> [`9f563c5..29cfecd`](#what-changed-on-dev-9f563c529cfecd) split logging out of
> the UART driver and gave every driver one way to reach a register.
> [`29cfecd..e34870f`](#what-changed-on-dev-29cfecde34870f) turned the exception
> code into the VMM, cut Log down to the debug console, and renamed the public
> API. Each section uses the names in force when it was written; the last one
> maps the old names to today's.

Three commits moved every MMIO address out of hand written headers and into the
device tree. Written as a refactor reference: what the new surface is, what is
gone, and what will bite.

```
aae62fa [bsp]     derive the MMIO windows from the device tree
dcb54e6 [bsp]     describe only the GIC frames a guest may reach
9f563c5 [drivers] take the GIC frame bases from the device tree
```

41 files, +664 / -455.

## The mental model

An address now reaches the code by one of three routes. If you are adding one,
pick the route first.

**Generated at configure time.** `tools/bspgen` reads the board's host DTB and
writes `regs.inc`: five bases and five sizes, as `BSP_*` macros with a `ULL`
suffix. This route is only for values that must be compile time constants,
which today means the console base (needed before any tree is parsed) and the
four GIC frame bases (the driver's register tables are built from them).
`verifyBspAgainstDtb` rechecks all ten against the firmware tree at boot and
panics on a mismatch.

**Read from a tree at runtime.** Everything else. `dtbHostMmio` and
`dtbGuestMmio` in `core/dtb/dtbMmio.cpp` decide which peripherals appear and
hand the windows to the MMU layers. This is where policy lives.

**Hand written because no tree can state it.** `bsp/fvp/platform.inc` only:
load addresses are where the model is *told* to put a blob.

## New surface

```cpp
// core/dtb/dtbMmio.h  — policy: which peripherals appear
MmioMap dtbHostMmio(uintptr_t dtb);        // console + all four GIC frames, 2 MiB blocks
MmioMap dtbGuestMmio(uintptr_t guestDtb);  // GICD + GICC + console, 4 KiB pages

// core/mm/mmu/mmioMap.h  — the shape, owned by Mm so Core can fill it
struct alignas(16) MmioWindow { uint64_t base, pa, size; bool byPage; };
struct alignas(16) MmioMap {
    bool addBlocks(uint64_t base, uint64_t size);            // identity, widened to 2 MiB
    bool addPages(uint64_t ipa, uint64_t pa, uint64_t size);  // 4 KiB, ipa may differ from pa
    bool covers(uint64_t addr) const;
    bool add(const MmioWindow& want);                         // idempotent
};
#define MMIO_MAX_WINDOWS 8

// core/dtb/dtb.h  — one home for the compatible lists
DeviceNode dtbFindUart(uintptr_t dtb);
DeviceNode dtbFindGic(uintptr_t dtb);   // regions[0..1] guest visible, [2..3] EL2 only

// drivers/gic/gic.h  — frames are runtime state
static void setBases(uint64_t dist, uint64_t cpu, uint64_t hv, uint64_t vcpu);
[[nodiscard]] static uint64_t distBase();
[[nodiscard]] static uint64_t hvBase();
[[nodiscard]] static uint64_t vcpuBase();
```

Changed signatures:

```cpp
HostMmu::init(const MmioMap& devices);                                   // was init()
GuestMmu::init(ipaBase, hostPaBase, sizeBytes, const MmioMap& devices);  // was 3 args
Vm::init(name, ipaBase, hostPa, size, vmid, entry, dtb, const MmioMap&); // was 7 args
Gic::reg(Frame frame, uintptr_t offset);                                 // was reg(absolute)
```

`LoadedGuest` gained `dtbHostPa`, so the guest tree is readable by EL2 without
redoing the IPA arithmetic.

## Gone

All three `bsp/<board>/bsp.h` are deleted, and with them the whole `b::`
namespace. Consumers include `"regs.inc"` and use `BSP_*` directly; the board
still selects itself, because that was always the include path and never the
filename.

Deleted symbols, in case you go looking: `HV_MMIO_BASE`/`_SIZE`,
`PLATFORM_MMIO_BASE`/`_SIZE`, `GIC_ITS_MMIO_BASE`/`_SIZE`, `GIC_BASE`,
`MMIO_REGION_SIZE`, `MMIO_PAGE_SIZE`, `GUEST_MMIO`, `GUEST_MMIO_PAGES`,
`GUEST_MMIO_COUNT`, `GUEST_MMIO_PAGE_COUNT`, `b::MmioRange`, `HOST_DTB_BASE`,
`GicReg::Vcpu::BASE`.

`Mm` no longer depends on any board header. `hostMmu.h` dropped its
`#include "bsp.h"` and takes its windows from the caller, which is the layering
the linker was already trying to enforce.

## Landmines

**Structs built before the MMU is on must be 16 byte aligned.** This cost a
boot panic. `HostMmu::init` runs with `SCTLR_EL2.M=0`, where all memory behaves
as Device-nGnRnE and every access must be naturally aligned. The compiler
copies a 32 byte struct with 128 bit NEON pairs, which fault on an 8 byte
aligned address, temporaries included. `MemoryMap`, `DeviceNode`, `MmioWindow`
and `MmioMap` all carry `alignas(16)` for this reason. Add it to anything new
on that path. The symptom is `ESR_EL2` EC `0x25`, DFSC `0x21`.

**`dtbGuestMmio` caps the GIC at the first two regions on purpose.** Regions
2 and 3 are GICH and GICV. A guest that reaches GICH programs the list
registers that inject its own interrupts. The guest trees now declare only the
two frames a guest may see, so the cap is a guard against a tree edit, not a
correction of today's data. Do not "simplify" it into a full union.

**Guest windows are page granular, host windows are block granular.** Not
cosmetic: on qemu all four GIC frames live inside one 2 MiB block, so a block
mapped guest GIC window hands over GICH and GICV. `HostMmu::mapRange` only
installs 2 MiB blocks, hence `addBlocks` for the host side.

**Never block map and page map the same 2 MiB region in stage 2.** `walkL3`
returns `nullptr` when the L2 entry is already a block, and the mapping is
silently dropped with an error line. `covers()` exists to keep the bring up
console shim from colliding on rpi5, where that IPA is already the real UART.

**`MMIO_MAX_WINDOWS` and `DT_MAX_REGIONS` are both 8 and both truncate.** A
full `MmioMap` warns and drops the window. `DT_MAX_REGIONS` was 4, which was
exactly GICv2's region count with zero headroom.

**bspgen runs at configure time, not build time.** `execute_process`, no
`DEPENDS`. Editing a `.dts` or the generator needs `cmake --preset debug`, not
just a rebuild.

**`ULL` belongs in `regs.inc` and never in `platform.inc`.** The latter is
included from `bsp/fvp/boot.S` and the assembler will not take the suffix.

## Drift chains still maintained by hand

These are the remaining places where two copies of the same fact can diverge
with nothing to catch it.

`bsp/fvp/dts/host-fvp.dts` -> `bsp/fvp/dts/host-fvp.dtb` (checked in, read by
bspgen). Nothing regenerates the blob when the source changes. The boot panic
in `verifyBspAgainstDtb` is the only thing that catches it, and only for the
ten values it checks.

`bsp/rpi5/dts/guest-rpi5.dts` -> `boot/dtb/guest-rpi5.dtb` ->
`boot/profiles/guest.hvgbp`. No build rule at all. qemu and fvp both compile
their guest tree with `dtc` at build time; rpi5 does not, because its package
is flashed rather than built. All three agree as of `dcb54e6`. Giving rpi5 the
same `dtc` plus `mkguestpkg` rules would close this, and wants a card to test.

`boot/profiles/guest-qemu.hvgbp` is a build output written into the source
tree, so it shows dirty after every build.

## Verifying a refactor

```sh
just test-unit                     # 159 host cases
cmake --preset debug               # after any .dts or bspgen change
cat build/debug/fvp/generated/regs.inc
just test-integration              # 49 cases in QEMU
just qemu                          # the only path that runs dtbGuestMmio on a real guest tree
```

Two things worth knowing about coverage. The integration suite feeds
`GuestMmu::init` a synthetic `MmioMap`, so only `just qemu` exercises the real
guest tree derivation; look for the `[GuestMmu] Mapping N guest MMIO window(s)`
block, which should be 4 windows on qemu.

Everything above is verified on qemu only. rpi5 needs hardware and fvp needs
the model, so the fvp mapping fix that motivated `aae62fa` is reasoned rather
than observed.

---

# What changed on dev: 9f563c5..29cfecd

Six commits pulled logging out of the UART driver, gave every driver one way to
reach a device register, and taught the image to run global constructors. Same
format as above: what the new surface is, what is gone, and what will bite.

```
885e8bc [lib]     run the global constructors collected into .init_array
3d5547c [lib]     add typed accessors for device registers
6d42df9 [drivers] take the GIC registers through the MMIO accessors
529c78c [lib]     move formatting and the console API out of the UART driver
72c974e [lib]     compile the debug console out of release builds
29cfecd [docs]    describe the shape the code is expected to take
```

## The mental model

**A driver owns hardware and nothing else.** `Uart` knows how to bring a PL011
frame up and push one character. It does not know what a line is. Everything
about how a line is built lives in `Log`.

**A device register is reached one way.** `mmio::read<T>` and `mmio::write<T>`,
with the width named at the call site. No driver casts an address itself.

**Console output has two tiers, chosen at compile time.** Not severity levels.
Boot progress is debug and disappears from a release image. Panic output and
the test harness are always emitted.

## New surface

```cpp
// lib/log/log.h  -- the console
class Log {
    static void println(const char* fmt, ...);   // debug tier, gone under NDEBUG
    static void print(const char* fmt, ...);     // debug tier, gone under NDEBUG
    static void writeLine(const char* fmt, ...); // always
    static void write(const char* fmt, ...);     // always
    static void writeCh(char ch);                // always
    static void writeHex(uint64_t val);          // always, 16 digits, no 0x
};

// lib/mmio/mmio.h  -- device registers, width never deduced
template <typename T> T    read(uintptr_t addr);
template <typename T> void write(uintptr_t addr, T value);
template <typename T, typename Off> T    read(uintptr_t base, Off offset);
template <typename T, typename Off> void write(uintptr_t base, Off offset, T value);

// lib/cxxrt/cxxrt.h  -- C++ runtime support
void runGlobalConstructors();   // called once from hmain

// drivers/uart/uart.h  -- hardware only
static Uart& Uart::getInstance();
void setBase(uint64_t base);
[[nodiscard]] uint64_t getBase() const;
void putc(const char ch) const;

// drivers/gic/gic.h
static uintptr_t Gic::frameBase(Frame frame);   // was reg(Frame, offset)
```

`log::detail` is the old `uart::detail` format engine, moved unchanged.

## Gone

`Uart::print`, `Uart::println`, `Uart::writeHex`, `Uart::putc` as a static, the
whole `uart::detail` namespace, the global `hex[]` table, and `Uart::init()`.
The constructor programs the frame now, and `setBase()` reconfigures through
the same private `configure()`.

`Gic::reg(Frame, uintptr_t)` is now `frameBase(Frame)` and returns an address
rather than a pointer.

`tests/unit/uart/test_uart_format.cpp` is `tests/unit/log/test_log_format.cpp`.

`test_init_doesnt_hang` went with the `init()` it called, so UartHarness is six
cases and a full integration run reports 48, not 49.

## Landmines

**A singleton constructor must never reach back through its own accessor.** The
guard byte for a function local static is written only after the constructor
returns, and `-fno-threadsafe-statics` removes the `__cxa_guard_acquire` that
would catch recursive initialisation. `Uart::Uart()` logs its banner by calling
`putc` on `this`, deliberately not through `Log`, whose sink calls
`getInstance()`. Route it through `Log` and the board eats its 4 MiB EL2 stack
and faults before the console exists to say so.

**Keep `Uart` trivially destructible.** Give it a destructor and the compiler
emits references to `__cxa_atexit` and `__dso_handle`. `lib/cxxrt` defines both,
but nothing registered there ever runs.

**The debug tier has to stay inline in the header.** An out of line empty
function still pins every format string in `.rodata`, which defeats the point.
Check with `strings build/release/<board>/kernel8.img`.

**Anything on the panic path belongs to the always tier.** `panic.cpp`,
`registerDump.h` and `tap.h` use `writeLine`/`write`. Moving one back to
`println` makes a release build die silently.

**`mmio::write` will not deduce its width.** `mmio::write(addr, 0x7FF)` is a
compile error, on purpose. Name the width.

**`.init_array` needs `KEEP`.** Nothing references those initialisers by symbol
and `--gc-sections` is on.

**There is no C++ standard library.** `-nostdinc++` is set and clang's resource
directory ships only C headers, so `<cstdint>` and `<type_traits>` do not
resolve. Use `<stdint.h>` and the builtins `__is_same`, `__is_integral`,
`__is_enum`.

## Verifying a refactor

```sh
just test-unit                     # 159 host cases
just build
just test-integration              # 48 cases in QEMU
```

Console output should be unchanged outside the suite you touched. Build the
previous commit into a worktree, run both images, and diff the logs; that is
how this round was checked.

---

# What changed on dev: 29cfecd..e34870f

Fifteen commits. The exception code became the VMM and lost three trap path
bugs, Log shrank to the debug console, and the tree was brought into line with
STYLES.md: `//` comments, UPPER_CASE enumerators, an UpperCamelCase public API
with `Get` and `Set` prefixes, and brace initialization. Same format as above.

```
e3f6a7d [docs]    record the logging and MMIO refactor in DEV_TREE_CHANGES
f6f4dd9 [misc]    address review on the clang-format and clang-tidy commit
4ebfb1e [misc]    drop the style recipes from the justfile
9aae47e [core]    rename exceptions to vmm
9396e79 [core]    give the vmm its ESR decode and the frame panic
4b5509a [core]    tidy the vmm trap path
7437f34 [docs]    record the enumerator, initialization and comment rules
2e25eda [core]    resume the guest after an SMC and report unhandled calls
2b261b4 [lib]     let hv_panic dump the saved registers again
0e69154 [lib]     make Log the debug console and nothing else
d70ac67 [misc]    use // comments throughout
0a3a30f [misc]    name enumerators in UPPER_CASE
f889b36 [misc]    name the public API in UpperCamelCase
a13c139 [misc]    prefer direct initialization
e34870f [misc]    prefix getters with Get
```

120 files, +4041 / -4139. Most of that is the style sweeps, which are
mechanical.

## The mental model

**`core/vmm` is where a trap goes.** The EL2 vector table, the entry glue that
saves a frame, guest trap dispatch, HVC and PSCI, SMCCC and the ESR decode all
live there. `core/vcpu` still owns entering and leaving the guest; `core/vmm`
decides what a trap means.

**Log is the debug console and nothing else.** `Log::Println` and `Log::Print`
are the only way to print, and a release build has neither. The panic path is
not logging: `HvPanic` and `RegisterDump` format with `log::detail` and write to
`Uart::Putc` themselves, so a release panic still reports.

**The public API is UpperCamelCase.** Public member functions and functions with
external linkage are UpperCamelCase, getters are `GetX` and setters are `SetX`.
Private methods and file local helpers stay lowerCamelCase, with `getX` and
`setX`. `.clang-tidy` enforces the public half. Names the assembly, compiler or
runtime reach by symbol keep their spelling, as do the lib pieces that mirror the
standard library.

## New surface

```cpp
// core/vmm/esr.h  -- the saved frame and the ESR_EL2 decode
enum class EsrEc : uint8_t;                    // HVC_AARCH64, SMC_AARCH64, DATA_ABORT_LOWER, ...
using ExceptionContext = hv::array<uint64_t, 31>;
constexpr EsrEc    GetEsrEc(uint64_t esr);     // bits [31:26]
constexpr uint32_t GetEsrIss(uint64_t esr);    // bits [24:0]

// core/vmm/vmm.h  -- trap entry points, extern "C", reached from vectors.S and vcpu.S
void handle_el2_sync(ExceptionContext& ctx);          // also _irq, _fiq, _serror, handle_unhandled
void handle_lower_el_sync(Vcpu* vcpu, uint64_t esr);  // also _irq, _fiq, _serror

// core/vmm/hvc/hvc.h
enum class HvcResult : uint8_t { HANDLED, UNHANDLED, HALT, RESET };
HvcResult HandleHvcAarch64(ExceptionContext& gpr);    // writes NOT_SUPPORTED to x0 on UNHANDLED

// core/vmm/smccc/smccc.h
constexpr uint64_t SMCCC::ToRegister(int32_t code);   // sign extends a return code for x0

// lib/log/log.h
class Log {
    static void Println(const char* fmt, ...);        // gone under NDEBUG
    static void Print(const char* fmt, ...);          // gone under NDEBUG
};
void log::detail::FormatLineToSink(Writer&& writer, const char* fmt, ...);

// lib/panic/panic.h
[[noreturn]] void HvPanic(const char* msg);
[[noreturn]] void HvPanic(const char* msg, const hv::array<uint64_t, 31>& ctx);
```

Renames you will hit first:

| Was | Now |
| --- | --- |
| `core/exceptions/` | `core/vmm/` |
| `exceptions.h`, `exceptions.cpp`, `exceptions.S` | `esr.h` and `vmm.h`, `vmm.cpp`, `vmm.S` |
| `tests/*/exceptions/`, `exceptionState.h` | `tests/*/vmm/`, `trapState.h` |
| `hv_panic`, `registerDump` | `HvPanic`, `RegisterDump` |
| `Log::println`, `Log::print` | `Log::Println`, `Log::Print` |
| `Uart::getInstance`, `putc`, `setBase`, `getBase` | `GetInstance`, `Putc`, `SetBase`, `GetBase` |
| `mmio::read`, `mmio::write` | `mmio::Read`, `mmio::Write` |
| `Gic::distBase`, `hvBase`, `vcpuBase`, `frameBase` | `GetDistBase`, `GetHvBase`, `GetVcpuBase`, `getFrameBase` |
| `VcpuLayoutAccess::gprOffset` and friends | `GetGprOffset` and friends |
| `Gic::init`, `Vm::init`, `Vcpu::init`, `pmm::init` | `Init` |
| `parseDtb`, `dtbHostMmio`, `dtbGuestMmio`, `verifyBspAgainstDtb` | `ParseDtb`, `DtbHostMmio`, `DtbGuestMmio`, `VerifyBspAgainstDtb` |
| `runGlobalConstructors`, `TestRunner::run_all` | `RunGlobalConstructors`, `TestRunner::RunAll` |
| `EsrEc::HvcAarch64`, `ValidateError::BadMagic`, `Gic::Frame::Hv` | `HVC_AARCH64`, `BAD_MAGIC`, `HV` |

The rest follow the same rule: a public name gains a capital, and an
UpperCamelCase enumerator becomes UPPER_SNAKE_CASE. Rebase a branch and the
compiler names every call site that still uses an old spelling.

## Gone

`Log::writeLine`, `Log::write`, `Log::writeCh` and `Log::writeHex`. Hex is
`{:x}`, which prints `0x` and does not zero pad, and a character is `{}`.
`log.cpp` holds only the sink.

The duplicate `using ExceptionContext` in `hvc.h`. `panicWithFrame` existed
briefly and went back into `HvPanic`.

The `EsrEc` values that never matched the Arm ARM. The coprocessor classes were
off by one, `0x07` was listed twice, and `0x1C` was called the PAC trap; it is
FPAC, and the PAC trap is `0x09`.

The `fmt`, `fmt-check`, `tidy`, `tidy-diff` and `lint-report` justfile recipes.
`scripts/lint-report.sh` stays for the CI rework.

## Behaviour changes

**A guest SMC resumes the guest.** It used to skip the instruction and return
into the `b .` after the call in `vcpu_exit_sync`, so the guest never ran again.
It now reads `NOT_SUPPORTED` back in x0.

**An unimplemented HVC returns `NOT_SUPPORTED`.** x0 used to keep the function
ID, which the guest reads back as its result. Vendor hypervisor calls now log in
a debug build, like every other unsupported owner.

**A guest SError panics.** It used to log through the debug console and spin,
so a release build stopped without a word.

**The register dump masks all 25 ISS bits**, not 24, and prints through the
formatter, so values carry `0x` and are not padded to 16 digits.

## Landmines

**Lib must not include core.** `ExceptionContext` lives in `core/vmm/esr.h`, so
`lib/panic` and `lib/registerDump` take `hv::array<uint64_t, 31>`, which is the
same type. The linker cannot see a header include, so this one is on review.

**Only the panic path prints in a release build, and it must not use Log.**
Route `HvPanic` or `RegisterDump` through `Log::Println` and a release panic
halts silently. Check with `strings build/release/qemu/kernel8.img`.

**The TAP harness prints through `Log::Println`.** Integration images are always
debug builds. A release built integration image prints no verdict, and CI times
out rather than passing.

**ABI names keep their spelling.** `hmain`, `vcpu_enter`,
`vcpu_save_el1_sysregs`, `vcpu_restore_el1_sysregs`, the `handle_*` trap entries,
`memcpy`, `memset` and `__cxa_*` are reached by symbol from assembly, the
compiler or the runtime, and renaming one breaks the link. `.clang-tidy` skips
them by pattern, so a new `extern "C"` entry point wants a `handle_*` or `vcpu_*`
name, or an entry in `GlobalFunctionIgnoredRegexp`.

**`hv::array`'s `begin`, `end`, `size` and `data` stay lowercase.** Range based
`for` needs `begin` and `end` by those exact names. The smart pointers' `get`,
`reset` and `release`, and `hv::move`, `forward` and `swap`, stay lowercase to
read like the std types they replace.

**A rename sweep over text will hit the linker scripts.** The pass that updated
comments matched `.start` in `KEEP(*(.text.start))` and turned it into
`.text.Start`, which links cleanly and does not boot. It was caught before the
commit. Keep renames out of `.ld` files, or check that `KEEP` still names the
section `boot.S` declares.

**Braces refuse narrowing.** `uint32_t ec { (esr >> 26) & 0x3F }` is an error
and needs a `static_cast<uint32_t>`. Lambdas keep `=`, and default arguments
cannot take braces.

**`Log::Println(const char* str)` prints the string as is.** It passes the
string through `"{}"`, so braces in a plain message are safe. The overload that
takes arguments treats braces as placeholders.

## Verifying a refactor

```sh
just test-unit                     # 164 host cases
just build
just build release
just test-integration              # 48 cases in QEMU
strings build/release/qemu/kernel8.img | grep "HV PANIC"   # the panic path survived release
```

The style sweeps were checked by hashing every `kernel8.img` before and after.
The comment, enumerator and brace initialization commits left all nine images
byte identical. A rename changes symbol names, so after one run the suites
instead. No test triggers a panic, so the register dump's output has not been
observed since it moved to the formatter.

Everything above is verified on qemu only.
