---
type: Architecture
title: Layout oddities
description: Files that are not where the layout rules say, and places where the README, AGENTS.md or another doc disagrees with the code. Nothing here has been changed.
resource: ../../
tags: [architecture, cleanup]
status: unverified
generated:
  by: claude-code/claude-sonnet-5-5
  at: 2026-10-06T12:00:00Z
sources:
  - resource: ../../AGENTS.md
  - resource: ../../README.md
  - resource: ../../CMakeLists.txt
  - resource: ../../justfile
  - resource: ../../.gitignore
  - resource: ../../.clangd
  - resource: ../../bsp/rpi5/linker.ld
  - resource: ../../boot/dts/guest-linux.dtsi
  - resource: ../../docs/sphinx/requirements.txt
  - resource: ../../docs/guides/TESTING.md
  - resource: ../../docs/guides/GUEST_ARCHIVE.md
  - resource: ../../scripts/bspgen/README.md
  - resource: ../../.github/workflows/integration.yml
---

# What this page is

A list for the owner to choose from. Each item is a place where the tree disagrees with its own rules or with something written about it. They were found by reading the files named, and none has been fixed.

# Files not where the rules put them

1. **`boot/dts/guest-linux.dtsi` is outside `bsp/`.** AGENTS.md says `bsp/<board>/` holds everything a board owns, and the shared guest tree is the only source file left in `boot/`. The build adds `boot/dts` to the `dtc` include path for it. Its own header says `@ingroup bsp`. GitHub issue 32 plans to move it into `bsp/`. The `boot/` folder was made on 2026-03-26 for the Pi firmware, and commit 2999987 moved the firmware out and left the tree behind. See [bsp](../subsystems/bsp.md).
2. **`Image` sits at the repo root.** It is a 13 MB tracked Linux kernel, and `CMakeLists.txt` uses it as the default `RPI5_GUEST_KERNEL`. AGENTS.md does not list it. GUEST_ARCHIVE.md says generated archives do not belong in source control, which is about archives, not this file.
3. **The host tree is in `dts/` for `qemu` and in `firmware/` for `rpi5`.** `bsp/qemu/dts/host-qemu.dtb` is the QEMU capture, and `bsp/rpi5/firmware/bcm2712-rpi-5-b.dtb` is the Pi's firmware tree. `HOST_DTB_qemu` and `HOST_DTB_rpi5` in `CMakeLists.txt` name them.
4. **`CONTEXT.md` and `docs/TODO.md` are not in AGENTS.md's layout.** `CONTEXT.md` holds domain terms and `docs/TODO.md` the open work.
5. **`lib/` is more than "panic, strings, and header only utilities".** It also has compiled `cpio`, `cxxrt` and `log`. `mmio` and `registerDump` are the header only ones.
6. **The vector table and entry assembly belong to `Virt` by folder but link into the image directly.** The reason is in a comment in `CMakeLists.txt`. See [layering](layering.md).

# Claims that disagree with the code

7. **README names `docs/requirements.txt`.** It does not exist. The file is `docs/sphinx/requirements.txt`, with `sphinx`, `breathe` and `furo`. The docs workflow installs those three by name and does not read either path.
8. **AGENTS.md lists `bsp.h` in each board folder.** No `bsp.h` exists. Commit d4ddde1 deleted them, and the code includes `regs.inc`.
9. **`scripts/bspgen/README.md` says "the BSP header and `boot.S` share one definition".** There is no BSP header, and `boot.S` does not include `regs.inc`. Only C++ files include it.
10. **README says the build produces `hyperberry.elf`.** The executable target is `hyperberry-<board>`, and `set(ELF hyperberry.elf)` is never used.
11. **README says `just rpi5` defaults to `/dev/sda1`.** The justfile default is `/dev/sdd1`.
12. **README and `just test-integration [qemu]` suggest a board argument.** Only `qemu` has a `run-<board>-test` target, so `just test-integration rpi5` fails at that step.
13. **README says the unit tests use the hosted `aarch64-linux` toolchain.** `cmake/aarch64-linux-toolchain.cmake` returns early unless `CI` is set, so local runs are host native.
14. **README links `docs/TESTING.md` and `docs/GUEST_ARCHIVE.md`, and AGENTS.md links `docs/STYLES.md`.** All three moved to `docs/guides/`, and the move is staged in the working tree. The links in README and AGENTS.md still use the old paths.
15. **TESTING.md describes an older layout.** It says to add a suite to an `INTEGRATION_TEST` block in the root `CMakeLists.txt`. The list is `SHARED_TESTS` in `tests/integration/CMakeLists.txt`. It names `tests/integration/uart_hw/test_uart_hw.cpp`, which is now `tests/integration/uart/test_uart.cpp`, and `tests/unit/mmu/test_mmu.cpp`, which does not exist. It also says the test sources are compiled into `hyperberry.elf`, where they are now the `Tests-<board>` library.
16. **GUEST_ARCHIVE.md lists `tests/vcpu.bin` and `tests/gic.bin`.** The integration archive also has `tests/abort.bin`.
17. **AGENTS.md says there are 159 unit cases.** A count of `TEST(` and `TEST_F(` lines gives 111. I did not run `ctest`.
18. **`bsp/rpi5/linker.ld` disagrees with itself.** The header says the firmware loads the image at `0x00200000` and reserves 32 KB. The `MEMORY` block uses `0x00080000` and a 512K reserved region.
19. **`bsp/qemu/linker.ld` says 4 GB, and `run-qemu` passes `-m 8G`.** The CI script passes `-m 4G`.
20. **`.gitignore` keeps `!boot/start4.elf`.** No such file exists. The Pi firmware is `bsp/rpi5/firmware/start4.elf`, which `*.elf` would ignore, so it is tracked only because it was added anyway.
21. **`.clangd` points integration tests at `build/integration-test`.** That directory is not made by any preset. The debug preset builds the integration images under `build/debug`.
22. **`integration.yml` builds `guest-archive-qemu` and then runs with the test archive.** The extra target is not used by the run.
23. **The glossary says drivers each expose one instance through `GetInstance()`.** `Uart` and `Gic` do. `Timer` has a public constructor and no `GetInstance`. The allocators `Pmm`, `Heap` and `HostMmu` do.
