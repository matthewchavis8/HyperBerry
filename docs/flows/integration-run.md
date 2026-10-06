---
type: Flow
title: Integration run
description: From just test-integration, or a CI pull request, to a PASS or FAIL verdict read off the QEMU serial output.
resource: ../../.github/actions/run-qemu-integration/
tags: [flow, tests, ci, qemu]
status: unverified
generated:
  by: claude-code/claude-sonnet-5-5
  at: 2026-10-06T12:00:00Z
sources:
  - resource: ../../justfile
  - resource: ../../CMakeLists.txt
  - resource: ../../tests/integration/CMakeLists.txt
  - resource: ../../tests/integration/suite.cpp
  - resource: ../../tests/integration/tap/tap.h
  - resource: ../../core/main.cpp
  - resource: ../../.github/workflows/integration.yml
  - resource: ../../.github/actions/run-qemu-integration/action.yml
  - resource: ../../.github/actions/run-qemu-integration/run_qemu_integration.py
---

# Summary

The integration image is a normal hypervisor image whose `hmain` runs every registered test suite instead of loading a guest. QEMU boots it, the suites print over the UART, and a small Python program watches that output for the words `TESTS PASSED` or `TESTS FAILED`. The same program is used by `just test-integration qemu` and by the pull request workflow. See [tests](../architecture/tests.md) for how the suites are laid out.

# Steps

1. **Start.** Locally, `just test-integration qemu` runs `cmake --preset debug` in the container, builds `hyperberry-qemu-test`, then builds `run-qemu-test`. In CI, `.github/workflows/integration.yml` does the same on a pull request to `main`, in the published toolchain image: `cmake --preset debug`, then the `hyperberry-qemu-test` and `guest-archive-qemu` targets. The `debug` preset has `BUILD_INTEGRATION=ON`, which `release` does not.
2. **Build the test image.** `hb_add_image` links `CoreTest-qemu` (the core compiled with `INTEGRATION_TEST`), `Tests-qemu` with `--whole-archive`, and `tests/integration/suite.cpp`. The result is `build/debug/qemu/integration/kernel8.img`.
3. **Build the guest payloads and archive.** The `test-payloads` target assembles `vcpu.bin`, `gic.bin` and `abort.bin` from `tests/integration/<name>/guest_payload.S`. The `test-archive-qemu` target packs them with `linux/Image`, `linux/guest.dtb` and `linux/initrd` into `build/debug/qemu/integration/guest.cpio`. It is a dependency of the test image.
4. **Launch QEMU.** `run-qemu-test` runs `.github/actions/run-qemu-integration/run_qemu_integration.py` with `--kernel`, `--initrd` and `--log`. In CI the composite action `.github/actions/run-qemu-integration` runs the same script, passing its inputs `kernel`, `initrd`, `log` and `timeout`. The timeout defaults to 120 seconds. The script starts `qemu-system-aarch64 -machine virt,virtualization=on,gic-version=2 -cpu cortex-a76 -m 4G -nographic -kernel <kernel> -initrd <initrd>`.
5. **Boot.** `boot.S` runs, then `hmain` brings up the device tree parse, the PMM and the host MMU, as in a normal boot. Because `INTEGRATION_TEST` is defined it then calls `TestRunner::SetBootContext` with the memory map and `TestRunner::RunAll()`, and no guest is loaded.
6. **Run the suites.** `RunAll` walks the `.hyperberry_tests` section. For each suite it prints a header with the case count, then one PASS or FAIL line per case. A case is a function returning `bool`.
7. **Print the verdict.** After the last suite it prints the totals and then `TESTS PASSED` if no case failed and `TESTS FAILED` otherwise. It then spins forever.
8. **Watch the output.** The Python script reads QEMU's combined output line by line, echoes each line to the terminal, and writes it to the log file. On the first line containing `TESTS PASSED` it records a pass, terminates QEMU and stops. On `TESTS FAILED` it records a failure the same way.
9. **Time out.** If no verdict line arrives within the timeout, it logs an error, ends QEMU and returns 124. The timeout is checked as each output line arrives.
10. **Exit.** The script returns 0 on a pass and 1 on a failure. If QEMU exits first, it returns QEMU's own nonzero code, or 1 if QEMU exited cleanly without a verdict. The workflow step fails on any nonzero code.
11. **Keep the log.** On failure, the workflow uploads `build/debug/qemu/integration/*.log` as the artifact `qemu-serial-logs`.

# Things to know

- The Pi is not run in CI. For it, `cmake --build --preset debug --target flash-rpi5-test` copies the test image, archive and firmware to the mounted SD card, and the results are read from a serial console.
- The CI workflow builds `guest-archive-qemu` and does not use it, since the run uses the test archive.
- The local target uses a script rather than QEMU's own exit, because the hypervisor never exits. It spins, and the script ends QEMU when it sees a verdict.
