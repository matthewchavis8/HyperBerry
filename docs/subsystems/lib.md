---
type: Subsystem
title: Lib
description: The bottom layer, holding console logging, typed MMIO access, the fatal panic path, freestanding memory helpers, the CPIO reader, global constructor support and the register dump.
resource: ../../lib/
tags: [lib, cpp]
status: unverified
generated:
  by: claude-code/claude-sonnet-5-5
  at: 2026-10-06T12:00:00Z
sources:
  - resource: ../../lib/CMakeLists.txt
  - resource: ../../lib/log/log.h
  - resource: ../../lib/log/log.cpp
  - resource: ../../lib/mmio/mmio.h
  - resource: ../../lib/panic/panic.h
  - resource: ../../lib/panic/panic.cpp
  - resource: ../../lib/strings/strings.h
  - resource: ../../lib/strings/strings.cpp
  - resource: ../../lib/cpio/cpio.h
  - resource: ../../lib/cpio/cpio.cpp
  - resource: ../../lib/cxxrt/cxxrt.h
  - resource: ../../lib/cxxrt/cxxrt.cpp
  - resource: ../../lib/registerDump/registerDump.h
  - resource: ../../core/main.cpp
---

# What it is

The lowest of the five libraries, the one every other layer may use. It holds seven small units, each
in its own folder under `lib/`: `log` (the console and its brace style formatter), `mmio` (typed
register access), `panic` (the fatal path), `strings` (the memory and string primitives a bare metal
build has to supply itself), `cpio` (a validating reader for the guest archive), `cxxrt` (running
global constructors) and `registerDump` (printing saved registers). The CMake target is
`Lib${BSP_SUFFIX}`, a static library built from `cpio.cpp`, `cxxrt.cpp`, `log.cpp`, `panic.cpp` and
`strings.cpp`. `mmio` and `registerDump` are header only and compile into whoever includes them.

# Depends on

- [Drivers](drivers.md), in practice: `log.cpp`, `panic.cpp` and `registerDump.h` all call
  `Uart::GetInstance().Putc`. See the gotchas.
- `cpio.cpp` uses `StrEq` from the `strings` unit of this same library.
- The compiler and the board linker script: `cxxrt.cpp` reads `__init_array_start` and `__init_array_end`.

# Used by

Counted from `#include` lines in `core/`, `drivers/`, `lib/` and `tests/`.

- `log.h`: `core/main.cpp`, `core/deviceTree/deviceTree.cpp`, `core/vm/vm.cpp`, `core/vmm/vmm.cpp`,
  `core/vmm/hvc/hvc.cpp`, all of `core/mm` except the heap, plus the TAP header and four test files.
- `mmio.h`: only `drivers/uart/uart.cpp` and `drivers/gic/gic.cpp`.
- `panic.h`: `core/main.cpp`, `core/deviceTree/deviceTree.cpp`, `core/vmm/exceptions/exceptions.cpp`
  and one unit test. `core/mm/heap/heap.cpp` declares `HvPanic(const char*)` by hand instead of including it.
- `strings.h`: `core/bootLoader/bootLoader.cpp`, `core/mm/pageTable/pageTable.cpp`,
  `core/mm/mmu/guestMmu/guestMmu.cpp` and `lib/cpio/cpio.cpp`.
- `cpio.h`: `core/bootLoader/bootLoader.h` (the loader holds a `cpio::Archive`), `core/main.cpp`
  by way of the loader, and the unit tests.
- `cxxrt.h`: `core/main.cpp` only.
- `registerDump.h`: `lib/panic/panic.cpp` only.

# Files

| File | What it does | Main types and functions | Called from |
|---|---|---|---|
| `lib/CMakeLists.txt` | Builds `Lib${BSP_SUFFIX}` and exports the `panic` and `strings` folders as public include paths | (none) | `core/CMakeLists.txt` |
| `lib/log/log.h` | Brace style formatter in `log::detail` and the `Log` console class | `Log::Println`, `Log::Print`, `log::detail::FormatLineToSink`, `log::detail::FormatToSink` | every layer above |
| `lib/log/log.cpp` | The one place `Log` touches the driver | `Log::sink` | (private to `Log`) |
| `lib/mmio/mmio.h` | Volatile register reads and writes with an explicit access width | `mmio::Read<T>`, `mmio::Write<T>`, each with a base plus offset form | `drivers/uart/uart.cpp`, `drivers/gic/gic.cpp` |
| `lib/panic/panic.h`, `lib/panic/panic.cpp` | Prints a banner and the fault registers, then halts forever | `HvPanic(msg)`, `HvPanic(msg, ctx)` | `core/deviceTree/deviceTree.cpp`, `core/vmm/exceptions/exceptions.cpp`, `core/main.cpp`, `core/mm/heap/heap.cpp` |
| `lib/strings/strings.h`, `lib/strings/strings.cpp` | `memcpy` and `memset` with C linkage, and two inline string tests | `memcpy`, `memset`, `StrEq`, `StrStartsWith` | `core/mm/pageTable/pageTable.cpp`, `core/mm/mmu/guestMmu/guestMmu.cpp`, `core/bootLoader/bootLoader.cpp`, `lib/cpio/cpio.cpp` |
| `lib/cpio/cpio.h`, `lib/cpio/cpio.cpp` | Validates a `newc` CPIO archive in place and looks files up in it | `cpio::Archive`, `cpio::Entry`, `cpio::File`, `cpio::Error`, `Archive::Next`, `Archive::Find`, `Archive::GetError` | `core/main.cpp`, `core/bootLoader/bootLoader.h` |
| `lib/cxxrt/cxxrt.h`, `lib/cxxrt/cxxrt.cpp` | Walks `.init_array` and calls each constructor | `RunGlobalConstructors` | `core/main.cpp` |
| `lib/registerDump/registerDump.h` | Prints the EL2 syndrome registers and x0 to x30 straight to the UART | `RegisterDump` | `lib/panic/panic.cpp` |

# Entry points

## log

`Log::Println(fmt, args...)` and `Log::Print(fmt, args...)`. Placeholders are `{}` and `{:x}` (or
`{:X}`); `{{` and `}}` print a literal brace. Both functions are inline and their bodies exist only
when `NDEBUG` is not defined, so a release image carries no log call and no format string. Hex output
always has a `0x` prefix and uppercase digits. Supported argument types are integers, `bool`, `char`,
`const char*`, `char*`, `std::string_view`, pointers and enums; anything else is a `static_assert`
failure at compile time.

## mmio

`mmio::Read<uint32_t>(base, offset)` and `mmio::Write<uint32_t>(base, offset, value)`. The access
width is the template argument and must be 1, 2, 4 or 8 bytes wide. On `Write` the value type is not
deduced, so the width is always written at the call site. The offset may be a plain integer or an
enum, which is how `UART_REG` in `drivers/uart/uart.h` is used.

## panic

`HvPanic("message")` for a fault with no saved context, and `HvPanic("message", frame.x)` from an
exception handler that has a saved register array. Neither returns. Both end in `wfe` inside an
infinite loop.

## strings

`memcpy` and `memset` are `extern "C"`, so the compiler's own calls to them resolve here. `StrEq`
compares two C strings for equality and `StrStartsWith` tests a prefix.

## cpio

Construct `cpio::Archive archive { data, size }`, check `GetError()`, then `Find("linux/Image", file)`
or iterate with `Next(cursor, entry)` from `cursor = 0`. `core/main.cpp` builds it from
`memoryMap.cpioArchiveBase` and hands it to `BootLoader`.

## cxxrt

`RunGlobalConstructors()`, called as the first statement of `hmain`.

## registerDump

`RegisterDump(ctx)`, reached only through the two argument `HvPanic`.

# Gotchas

- **Lib calls up into Drivers.** `AGENTS.md` says each layer depends only on those below it, and Lib
  is the lowest, but `log.cpp`, `panic.cpp` and `registerDump.h` include `drivers/uart/uart.h` and
  call `Uart`. The `Lib` CMake target declares no link libraries, while `Drivers` links `Lib` as
  PUBLIC. The symbol is satisfied only because the final image also links `Drivers`.
- **Only `panic` and `strings` are on the include path.** `lib/CMakeLists.txt` exports those two folders. The
  other units are reached through the repository root, as `"lib/log/log.h"`, which is how every caller spells it.
- **`Log` vanishes in release, but `HvPanic` does not.** Panic formats through `log::detail` straight to the UART,
  so a release build still reports a panic. The same is true of `RegisterDump`. Do not rely on `Log` for anything
  a release image must say.
- **A call to `Log` compiles to nothing, but its arguments are still evaluated at the call site** unless the
  optimizer proves them free of side effects. Keep side effects out of log arguments.
- **A mismatched placeholder count prints into the line instead of failing.** Too few arguments print
  `[missing arg]`, too many print `[extra arg]`, and a stray brace prints `[invalid format]`.
- **`Println(const char*)` with one argument does not parse braces.** The single string overload prints through
  `"{}"`, so `Log::Println("{{")` prints two braces. With a second argument the format string is parsed.
- **Hex of a negative signed value sign extends.** `{:x}` casts to `uint64_t`, so a negative `int` prints sixteen
  hex digits. An enum prints as a signed decimal with `{}`.
- **The two `HvPanic` overloads print different registers.** The one without a context reads ESR_EL2, ELR_EL2,
  FAR_EL2, SPSR_EL2, HPFAR_EL2, VTTBR_EL2 and VTCR_EL2 live, so they describe the most recent exception, not
  necessarily the one that led to the panic. The one with a context calls `RegisterDump`, which prints ESR, EC, ISS,
  ELR, SPSR and FAR also read live, plus x0 to x30 from the array, and does not print HPFAR, VTTBR or VTCR.
  The comment on `RegisterDump` says ELR and SPSR come from the saved context, but the code reads the registers.
- **`mmio` has no barriers.** The header says ordering a device write against normal memory needs an explicit
  `dsb` at the call site. It also says not to use it on blobs in RAM, such as the device tree.
- **`memcpy` is a forward byte loop with no overlap handling.** There is no `memmove`, `memcmp` or `strlen` here.
- **The CPIO reader validates the whole archive in its constructor and rejects it on the first problem.**
  A bad archive leaves `GetError()` nonzero, and `Next` and `Find` then return false for every call. The checks include:
  the `070701` magic, all 13 header fields being hex, the check field being zero, names that are NUL terminated and
  padded with zeros, no empty, `.` or `..` path parts, no absolute paths, no duplicate names (a check that compares
  every entry with every earlier entry), and a `TRAILER!!!` entry followed only by zero bytes. A regular file must have a link
  count of 1, so a hard link, a symlink or a device node makes the whole archive unusable with `UNSUPPORTED_TYPE`.
- **`cpio::Archive` borrows the bytes.** Entry names and file data are pointers into the original buffer, which
  must stay alive and unchanged. `BootLoader` keeps a reference to the archive, so the archive object has to outlive the loader.
- **An `Archive` made from a null pointer or a range that wraps the address space keeps `INVALID_REGION`.**
  That is the initial value of its error field, not a state a read can set.
- **`Find` strips a leading `./` from the query and from entry names, skips directories, and never matches `.`.**
- **Global constructors run only because `hmain` asks.** `RunGlobalConstructors` must come first, and constructors run in
  link order, so one must not depend on another translation unit's global. The singletons avoid this by
  using function local statics through `GetInstance()`.
