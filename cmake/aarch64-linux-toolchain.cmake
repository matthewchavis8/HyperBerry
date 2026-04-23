# macOS lacks the aarch64-linux-gnu sysroot (Scrt1.o, crti.o, crtbeginS.o
# come from libc6-dev-arm64-cross, which has no Darwin equivalent). Unit
# tests are pure logic — let macOS build them host-native instead.
if(CMAKE_HOST_APPLE)
  return()
endif()

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_C_COMPILER   clang)
set(CMAKE_CXX_COMPILER clang++)

set(CMAKE_C_COMPILER_TARGET   aarch64-linux-gnu)
set(CMAKE_CXX_COMPILER_TARGET aarch64-linux-gnu)

add_compile_options(--gcc-toolchain=/usr)
add_link_options(--gcc-toolchain=/usr -fuse-ld=lld)

set(CMAKE_FIND_ROOT_PATH /usr/aarch64-linux-gnu)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

set(CMAKE_CROSSCOMPILING_EMULATOR qemu-aarch64 -L /usr/aarch64-linux-gnu)
