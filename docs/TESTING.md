# Testing

This project has two automated test paths:

- **Unit tests** under `tests/unit/` — hosted GoogleTest suite, cross-compiled with the `aarch64-linux` toolchain and run through `ctest`.
- **Integration tests** under `tests/integration/` — bare-metal tests that run on the actual target (QEMU virt or Raspberry Pi 5) and emit results over UART.

## Running Unit Tests

```sh
just test-unit
```

That recipe does three things:

```sh
cmake --preset unit-tests
cmake --build --preset unit-tests
ctest --preset unit-tests
```

The `unit-tests` preset uses:

- `build/unit-tests` as the build directory
- `cmake/aarch64-linux-toolchain.cmake`
- `BUILD_TESTING=ON`
- `ctest --preset unit-tests` with `outputOnFailure` enabled

## Running Integration Tests

```sh
just test-integration qemu          # run in QEMU
just test-integration rpi5          # flash and run on Raspberry Pi 5
just test-integration rpi5 /dev/sdX1  # specify SD card partition
```

The `integration-test` CMake preset inherits from `debug` and sets `INTEGRATION_TEST=ON`. When that option is enabled, the integration test sources are compiled directly into the `hyperberry.elf` target and `hmain()` calls `TestRunner::run_all()` instead of the normal hypervisor path.

That recipe maps to:

```sh
cmake --preset integration-test -DBOARD=qemu
cmake --build --preset integration-test
cmake --build --preset integration-test --target run
```

For Raspberry Pi 5, `just` also mounts `/mnt/sdcard`, flashes the generated `kernel8.img` plus firmware files, then unmounts the card again.

UART output looks like:

```
======== uart_hw (2 tests) ========
  [1/2] PASS: uart_hw::tx doesnt hang
  [2/2] PASS: uart_hw::rx loopback stub

-------- Results --------
2 passed, 0 failed, 2 total
ALL TESTS PASSED
```

## How Integration Tests Work

Integration tests use **linker-section auto-registration**:

1. Each test file defines a `TestSuite` with an array of `TestCase` entries.
2. `REGISTER_SUITE(suite)` places a pointer to that suite into the `.hyperberry_tests` linker section.
3. At boot, `TestRunner::run_all()` walks from `__test_suites_start` to `__test_suites_end`, executing every registered suite.
4. Results are emitted over UART via the `Tap` namespace (freestanding, no stdlib).
5. After all suites finish the CPU spins — there is no OS to return to.

## Doxygen Coverage

The integration test harness is documented with file-level and symbol-level Doxygen comments:

- `tests/integration/suite.h` documents the core test registration types and macro.
- `tests/integration/suite.cpp` documents the linker-section runner entry point.
- `tests/integration/tap/tap.h` documents the UART TAP-style output helpers.
- `tests/integration/uart_hw/test_uart_hw.cpp` documents the current UART smoke tests.

Keep that level of coverage when adding new test files: at minimum add an `@file` block and brief comments for any non-obvious test helpers or registration objects.

## Adding a New Integration Test

1. Create a new directory under `tests/integration/` (e.g. `tests/integration/gic/`).
2. Write a test file:

```cpp
#include "tests/integration/suite.h"

static bool test_something() {
  // exercise hardware, return true on pass
  return true;
}

static const TestCase my_cases[] = {
    {"something works", test_something},
};

static const TestSuite my_suite = {
    "gic",
    my_cases,
    1,
};

REGISTER_SUITE(my_suite);
```

3. Add the file to `CMakeLists.txt` inside the `INTEGRATION_TEST` block:

```cmake
if(INTEGRATION_TEST)
  add_compile_definitions(INTEGRATION_TEST)
  target_sources(${ELF} PRIVATE
    tests/integration/suite.cpp
    tests/integration/uart_hw/test_uart_hw.cpp
    tests/integration/gic/test_gic.cpp      # new
  )
endif()
```

4. Run `just test-integration qemu` to verify.

## Adding a New Unit Test

1. Create a new test file under `tests/unit/`.
2. Add that file to `hyperberry_unit_tests` in `tests/unit/CMakeLists.txt`.
3. Include the header or module you want to test.
4. Add normal GoogleTest `TEST(...)` or `TEST_F(...)` cases.
5. Run `just test-unit` to verify the new test is discovered and passes.

Example:

```cpp
#include <gtest/gtest.h>

#include "lib/array.h"

TEST(HvArrayExample, DefaultValue) {
  hv::array<int, 2> values{};
  EXPECT_EQ(values[0], 0);
  EXPECT_EQ(values[1], 0);
}
```

Then register the file in `tests/unit/CMakeLists.txt`:

```cmake
add_executable(hyperberry_unit_tests
  array/test_array.cpp
  your_module/test_your_module.cpp
)
```

## What Belongs Where

| Test type   | When to use                                         |
|-------------|-----------------------------------------------------|
| Unit        | Pure logic, utility types, header helpers, anything without hardware |
| Integration | UART, GIC, timers, MMU runtime APIs — anything that needs EL2    |

## Notes

- `BUILD_TESTING` short-circuits the main bare-metal build in the top-level `CMakeLists.txt`.
- `INTEGRATION_TEST` compiles tests into the bare-metal binary and replaces the normal hypervisor entry path.
- GoogleTest is fetched during the unit-test configure step.
- `just test-integration qemu` streams UART output directly in the terminal because the QEMU target uses `-nographic`.
- MMU tests are intentionally split: `tests/unit/mmu/test_mmu.cpp` covers header-level descriptor/index helpers, while `tests/integration/mmu/test_mmu.cpp` exercises the real EL2 MMU APIs and live page tables.
