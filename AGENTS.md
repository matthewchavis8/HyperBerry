# Working on HyperBerry

HyperBerry is a bare metal Armv8-A type 1 hypervisor running at EL2. Targets are
Raspberry Pi 5 (BCM2712), QEMU virt, and the Arm FVP Base RevC model.

## Build and verify

```sh
just build            # every board, debug
just qemu             # build and run in QEMU
just rpi5             # build and flash an SD card
just fvp              # build and run on the FVP model
just test-unit        # host tests, 159 cases
just test-integration # bare metal TAP suite, defaults to qemu
just fmt-check        # CI formatting gate
```

One configure builds every board. Images land in `build/<mode>/<board>/kernel8.img`,
with the integration image under `build/<mode>/<board>/integration/`. Flashing
picks the image it needs rather than reconfiguring.

Run `just fmt-check` and the integration suite before calling anything done.

## Layout

```
arch/aarch64/        shared assembly only (vectors, exceptions, vcpu)
bsp/<board>/         everything a board owns: bsp.h, boot.S, linker.ld, dts/, firmware/
core/                main, dtb, bootpkg, mm/, vcpu, vm, exceptions
drivers/             uart, gic, timer
lib/                 panic, strings, and header only utilities
tests/integration/   board neutral suites
tests/bsp/<board>/   board specific test sources
tools/bspgen/        generates regs.inc from a board's device tree
```

Libraries are five subsystems, layered. Each may depend only on those below it:
`Core` then `Virt` then `Mm` then `Drivers` then `Lib`. The linker enforces this,
so a layering violation shows up as a build failure rather than a review comment.
Target names carry a `-<board>` suffix because one CMake project cannot hold three
targets of the same name.

## Conventions

**The device tree is the source of truth for addresses.** `tools/bspgen` reads a
board's host device tree and emits `regs.inc`, which `bsp.h` wraps in constexpr.
Do not hand write a value the tree already declares. Boot checks the compiled
values against the firmware tree and panics on a mismatch.

**There is no board macro.** No `BSP_QEMU`, `BSP_RPI5` or `BSP_FVP`. Each board
ships its own `bsp.h` and the build puts that directory on the include path, so
code includes `"bsp.h"` and the board selects itself. Never reintroduce an
`#ifdef` on the board.

**Board specific test code goes in `tests/bsp/<board>/`.** A file there whose
basename matches one in `tests/integration/` replaces it for that board. Use this
when a suite needs a different implementation, not a different address. A
differing address should be passed in from the host side, the way the GIC payload
receives its GICV base in x0.

## How to work here

**Comments are minimal.** Write one only when the code cannot say it itself. Do
not narrate what a line does, do not leave a comment explaining that something was
removed, and do not restate an address or value that the code already declares.

**Name things what they are.** No vanity prefixes. The memory subsystem is `Mm`,
not `HbMm`. A prefix has to earn its place, which for a target name it does not.

**Commit messages state what changed and why. Nothing else.** No test counts, no
verification narrative, no "verified on QEMU", no line counts, no hedges about
what was not tested, no commentary about comments in the code, and no account of
approaches that were tried and abandoned. If it does not describe the change or
its reason, cut it.

**No hyphens in prose.** Applies to commit messages, docs and comments. Keep them
only inside literal identifiers such as `--gc-sections`, `host-fvp.dts` or
`arm,gic-400`, where removing them would be wrong.

**Say when you are deviating from the plan.** If the work drifts from what was
agreed, flag it before continuing rather than letting it land quietly.

**Prefer a conversation to a wall of questions.** Ask the one thing that actually
blocks the work instead of firing off several structured questions at once.

**Verify before claiming.** Do not report something as working on the strength of
a clean build. Run it. If a claim turns out to be wrong, say so plainly and move
on instead of defending it.
