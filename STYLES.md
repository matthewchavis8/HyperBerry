# C++ style

Companion to [AGENTS.md](AGENTS.md), which covers the build, the layout and how
to work here. This file is about the shape of the code itself. Everything below
was settled by review rather than invented, so treat a conflict with existing
code as the existing code being older, not as this file being wrong.

## Class shape

**`private` first, then `public`.** State comes before the interface that uses
it.

**The rule of five goes at the bottom of the class**, after the real API. It is
lifetime boilerplate, not something a reader looks for first.

**Close copy and move on anything that owns hardware.** Left implicit, the
compiler hands out a copy for `Uart u = Uart::getInstance();` and that copy
drives the same registers behind the singleton's back. All four go, and a
defaulted destructor keeps the type trivially destructible:

```cpp
Uart(const Uart&) = delete;
Uart& operator=(const Uart&) = delete;
Uart(Uart&&) = delete;
Uart& operator=(Uart&&) = delete;
~Uart() = default;
```

**Getters are named `getX()`.** `getBase()`, `getInstance()`. `Gic` still
carries `distBase()`, `hvBase()` and `vcpuBase()` from before this rule.

**Prefer a constructor over an init idiom.** A driver that needs register
programming does it in the constructor, through a private helper the rebind path
can also call. `Uart::Uart()` calls `configure()`, and so does `setBase()`.
There is no public `init()` to forget.

## Declarations

**Enumerators are UPPER_CASE.** `.clang-tidy` enforces it through
`readability-identifier-naming.EnumConstantCase`: `HvcResult::HANDLED`,
`EsrEc::DATA_ABORT_LOWER`.

**Prefer direct initialization.** Braces rather than `=`:

```cpp
inline constexpr uint32_t OEN_SHIFT { 24 };
uint64_t callId { gpr[0] };
```

Braces refuse a narrowing conversion that `=` accepts without a word.

## Singletons

One instance of a device is a function local static, not a namespace scope
object:

```cpp
Uart& Uart::getInstance() {
    static Uart console;
    return console;
}
```

It initialises on first call, not at static init time, so it works before the
heap exists. Two rules come with it.

**Keep the type trivially destructible.** A real destructor emits references to
`__cxa_atexit` and `__dso_handle`. `lib/cxxrt` defines both, but a hypervisor
that never exits has no use for a destructor that never runs.

**A constructor must never reach back through its own accessor.** The guard byte
is written only after the constructor returns, and `-fno-threadsafe-statics`
removes the `__cxa_guard_acquire` that would otherwise catch recursive
initialisation. So a constructor that logs through `Log`, whose sink calls
`getInstance()`, recurses until the stack is gone and faults with no console
output. Write through `this` instead, as `Uart::Uart()` does for its banner.

## Comments

**`//` line comments, never `/** */` blocks.** This holds everywhere: headers,
sources, tests and assembly.

**A short note on an enumerator, field or constant goes after it on the same
line**, not in a block above it:

```cpp
enum class HvcResult : uint8_t {
    HANDLED,   // handled, the guest can resume
    UNHANDLED, // an HVC exit, but the call is not implemented
};
```

**Every documented function carries `@return`**, including the ones returning
void.

Known consequence, unresolved: Doxygen does not read plain `//`. It recognises
only `///`, `//!`, `/** */` and `/*! */`, and no Doxyfile setting changes that.
So `@brief`, `@param` and `@return` written in `//` comments do not reach
`just docs`. Changing `//` to `///` would keep both.

## MMIO

**One accessor, and the width is named at the call site.** `lib/mmio` is the
only place a device register is turned into a pointer:

```cpp
mmio::write<uint32_t>(m_base, UART_REG::DR, value);
uint32_t fr = mmio::read<uint32_t>(m_base, UART_REG::FR);
```

Both an absolute address form and a base plus offset form exist. The width is
never deduced: the value parameter passes through a non deduced indirection, so
`mmio::write(addr, 0x7FF)` is a compile error rather than a silent `int` width
access. A `static_assert` rejects anything that is not an integral type of 1, 2,
4 or 8 bytes.

**No driver local `reg()` helpers.** Every driver was open coding the same cast.
A driver exposes the frame base it wants and hands that to `mmio`.

**`uintptr_t` for an address you are about to dereference, `uint64_t` while it
is still a value the device tree told you about.** `MmioWindow::base` and
`Uart::getBase()` are `uint64_t`; `mmio::read` and `mmio::write` take
`uintptr_t`.

## Logging

**`Log::println` and `Log::print` are the only way to print.** Both compile out
when `NDEBUG` is set, so boot progress and diagnostics vanish from a release
image, format strings included. There are no log levels. Integration images are
always debug builds, so the TAP harness prints through them too.

The calls are inline in the header on purpose. An out of line empty function
still pins every format string in `.rodata`.

**The panic path is not logging.** `hv_panic` and `registerDump` format with
`log::detail` and write straight to `Uart::putc`, so a release panic still
reports. A release build that panics silently is a release build you cannot
debug. Nothing else writes to the UART that way.

**Call sites qualify: `Log::println(...)`.** No free forwarders, no using
directive, one name per entry point.

**`Uart` is the hardware and `Log` is the console.** The driver knows how to push
one character. Everything about how a line is built lives in `Log`.

## Freestanding limits

**There is no C++ standard library.** `-nostdinc++` at `CMakeLists.txt` removes
it, and clang's resource directory ships only C headers. `<cstdint>`,
`<type_traits>` and `<utility>` all fail to resolve; use `<stdint.h>` and the
hand rolled subset in `lib/array`, `lib/memory` and `lib/utility`. For type
queries use the compiler builtins the format engine already uses: `__is_same`,
`__is_integral`, `__is_enum`.

**Global constructors run, but only because we run them.** Each linker script
bounds an `.init_array` block and `runGlobalConstructors()` in `lib/cxxrt` walks
it from `hmain`. Order inside that section is link order unless a constructor
carries `init_priority`, so a constructor must not depend on another translation
unit's global already being built.
