# Testing

This project currently has one working automated test path:

- Unit tests under `tests/unit/`

The `tests/integration/` tree exists, but its source files are currently empty and it is not wired into the top-level CMake flow yet.

## Current Test Layout

```text
tests/
├── integration/     # placeholder scaffold for target-level tests
└── unit/            # GoogleTest-based hosted unit tests
    ├── CMakeLists.txt
    └── unit_array/
        └── test_array.cpp
```

## Running Unit Tests

Use the `just` recipe:

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

## What Runs Today

At the moment, the active unit suite is:

- `tests/unit/unit_array/test_array.cpp`

It is built into the `hyperberry_unit_tests` executable and discovered through `gtest_discover_tests(...)`.

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
  unit_array/test_array.cpp
  your_module/test_your_module.cpp
)
```

## What Belongs in Unit Tests

Unit tests are the right fit for code that can be validated without booting the hypervisor:

- small utility types
- pure logic
- self-contained helpers
- code with minimal hardware coupling

If a module depends directly on EL2 state, boot flow, exception vectors, or real device behavior, it usually does not belong in this hosted unit-test target without first introducing a clean seam around that dependency.

## Integration Tests

`tests/integration/` is present for future target-level testing, but it is not active yet.

Right now there is no command in the repo that:

- builds integration tests
- deploys them to Raspberry Pi 5
- runs them as part of `ctest`

If you want integration tests to exist only when explicitly building for Raspberry Pi 5, the clean approach is to keep them behind a dedicated CMake option such as `BUILD_INTEGRATION_TESTS=ON` and only enable that path in a separate preset or `just` recipe. That keeps normal unit-test work fast and avoids mixing hosted tests with bare-metal target tests.

## Recommended Workflow

- Use `just test-unit` for normal development.
- Add unit coverage first for modules that do not need hardware.
- Keep target or board-specific tests separate from the hosted unit-test pipeline.
- Wire integration tests only when you are ready to define their boot, deploy, and pass/fail flow clearly.

## Notes

- `BUILD_TESTING` short-circuits the main bare-metal build in the current top-level `CMakeLists.txt`.
- GoogleTest is fetched during the unit-test configure step.
- As of April 2, 2026, `just test-unit` passes with 11 tests in this workspace.
