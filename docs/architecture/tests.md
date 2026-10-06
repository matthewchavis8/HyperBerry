---
type: Architecture
title: Tests
description: The unit suite and the bare metal TAP suites, how a board overrides a test, how each runs, and what the three CI workflows do.
resource: ../../tests/
tags: [architecture, tests, ci]
status: unverified
generated:
  by: claude-code/claude-sonnet-5-5
  at: 2026-10-06T12:00:00Z
sources:
  - resource: ../../AGENTS.md
  - resource: ../../CMakeLists.txt
  - resource: ../../justfile
  - resource: ../../tests/unit/CMakeLists.txt
  - resource: ../../tests/integration/CMakeLists.txt
  - resource: ../../tests/integration/suite.cpp
  - resource: ../../tests/integration/suite.h
  - resource: ../../tests/bsp/qemu/README.md
  - resource: ../../tests/bsp/rpi5/README.md
  - resource: ../../.github/workflows/unit.yml
  - resource: ../../.github/workflows/integration.yml
  - resource: ../../.github/workflows/docs.yml
  - resource: ../../.github/workflows/toolchain.yml
  - resource: ../../.github/actions/run-qemu-integration/action.yml
  - resource: ../../.github/actions/run-qemu-integration/run_qemu_integration.py
---

# The two paths

| | Unit | Integration |
|---|---|---|
| Where | `tests/unit/` | `tests/integration/`, with board files in `tests/bsp/<board>/` |
| Framework | GoogleTest | Own runner in `suite.cpp`, output in the TAP style over the UART |
| Runs on | A hosted AArch64 Linux or host native process | The bare metal image, at EL2, on QEMU or a Pi 5 |
| For | Pure logic and anything that does not need EL2 | Anything that touches hardware: UART, GIC, timer, the MMUs, the vCPU and the VMM |
| Command | `just test-unit` | `just test-integration` |

The long form is in the [testing guide](../guides/TESTING.md). That guide is partly out of date, see [layout oddities](layout-oddities.md).

# Unit tests

`tests/unit/CMakeLists.txt` builds two executables, `hyperberry_unit_tests` and `hyperberry_heap_tests`. They do not link the five libraries. Each lists the test files and the exact source files under test, for example `core/vmm/vmm.cpp`, `lib/cpio/cpio.cpp` and `core/bootLoader/bootLoader.cpp`. The heap tests are a separate executable because `heap.cpp`'s PMM stub conflicts with the stub the Vm tests use, and they define `HEAP_TESTING_BUILD`.

The unit configure generates `regs.inc` for `qemu` only. A `cpio_fixture` target builds `fixture.cpio` from `tests/unit/cpio/root` with `scripts/cpio/archive.py`. GoogleTest v1.14.0 is fetched at configure time, and `gtest_discover_tests` registers each case with CTest. `AGENTS.md` says there are 159 cases. A count of `TEST(` and `TEST_F(` lines under `tests/unit/` gives 111, and I did not run `ctest` to compare.

The toolchain file `cmake/aarch64-linux-toolchain.cmake` returns at the top unless the environment variable `CI` is set. Outside CI the unit tests build host native. In CI the target is `aarch64-linux-gnu`, run through `qemu-aarch64`.

# Integration tests

Each suite file defines a `TestSuite` of `TestCase` entries and calls `REGISTER_SUITE`, which puts a pointer in the `.hyperberry_tests` linker section. `TestRunner::RunAll()` in `suite.cpp` walks from `__test_suites_start` to `__test_suites_end`, prints one `[n/m] PASS` or `FAIL` line per case, prints the totals, and then spins forever. `hmain` calls it after the host MMU is up when `INTEGRATION_TEST` is defined, in place of loading a guest.

The suites listed in `SHARED_TESTS` are `uart`, `vmm`, `pmm`, `mmu`, `guestMmu`, `bootLoader`, `vcpu`, `gic` and `timer`. Some have an assembly file beside them for test vectors or a saved context. The `vcpu`, `gic` and `abort` folders also have a `guest_payload.S`, which is assembled into a flat binary and packed into the integration archive.

Each board builds `Tests-<board>` as a static library of those sources. The test image links it with `--whole-archive`, because nothing references a suite by symbol. The image links `CoreTest-<board>`, the `Core` variant compiled with `INTEGRATION_TEST`, in place of `Core-<board>`.

# Board overrides in tests/bsp

`tests/integration/CMakeLists.txt` globs `tests/bsp/<board>/*.cpp` and `*.S` with `CONFIGURE_DEPENDS`. A file there whose basename matches an entry in `SHARED_TESTS` replaces the shared one for that board, and the configure prints a line saying so. Any other file there is added to that board's test image. Both folders hold only a `README.md` today, so nothing is overridden.

AGENTS.md gives the rule for using this. Use it when a suite needs a different implementation, not a different address. A differing address should come from the host side, as the GIC payload gets its GICV base in `x0`.

# CI

| Workflow | Runs on | What it does |
|---|---|---|
| `unit.yml` | Every push except to `gh-pages`, and pull requests to `main` or `dev` | In the toolchain image, `just test-unit`. On failure it uploads `build/unit-tests/Testing/`. |
| `integration.yml` | Pull requests to `main` | Configures `debug`, builds `hyperberry-qemu-test` and `guest-archive-qemu`, then runs the `run-qemu-integration` action on the test image and its archive. On failure it uploads the serial log. |
| `docs.yml` | Push to `main` and pull requests | Doxygen, then Sphinx, then deploys `docs/_build/html` to `gh-pages` with `peaceiris/actions-gh-pages`. |
| `toolchain.yml` | Push to `main` or `dev` that changes the `Dockerfile` or the workflow, or by hand | Builds the `Dockerfile` and pushes `ghcr.io/matthewchavis8/hyperberry-toolchain:llvm-22.1.2`. |

The `setup-ci` composite action only checks that `clang-22` runs and that the libc++ and libc files are in the toolchain image. See [integration run](../flows/integration-run.md) for the QEMU step.

# Gotchas

- Only QEMU is run in CI. The Pi suite is run by hand through `flash-rpi5-test` and a serial console.
- Adding an integration test means a new line in `SHARED_TESTS` in `tests/integration/CMakeLists.txt`, not an edit to the root `CMakeLists.txt`.
- The `integration.yml` build of `guest-archive-qemu` makes the production archive, but the run uses the test archive. The test archive is already built as a dependency of `hyperberry-qemu-test`.
- `docs.yml` deploys on pull requests as well as on `main`.
