cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) 
set(CMAKE_OBJCOPY llvm-objcopy)
set(ELF hyperberry.elf)

project(HyperBerry VERSION 0.0.1 LANGUAGES CXX ASM C)

# Google Test
option(BUILD_TESTING "Build unit tests" OFF)
include(CTest)

if(BUILD_TESTING)
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

  include(FetchContent)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
  )
  FetchContent_MakeAvailable(googletest)

  add_subdirectory(tests/unit)
  return()
endif()

# Build option for targets
set(BOARD "rpi5" CACHE STRING "Target Board [rpi5 | qemu]")

if (BOARD STREQUAL "rpi5")
  message(STATUS "Building for target: Raspberry Pi 5")
  set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/linker/rpi5/linker.ld)
  set(BOOT_S        ${CMAKE_SOURCE_DIR}/arch/aarch64/rpi5/boot.S)
  add_compile_definitions(BSP_RPI5)
elseif (BOARD STREQUAL "qemu")
  message(STATUS "Building for target: Virtualized Raspberry Pi 5 in QEMU")
  set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/linker/qemu/linker.ld)
  set(BOOT_S        ${CMAKE_SOURCE_DIR}/arch/aarch64/qemu/boot.S)
  add_compile_definitions(BSP_QEMU)
else ()
  message(STATUS "[ERROR] unknown board detected={BOARD}")
endif()

include_directories(${CMAKE_SOURCE_DIR})

add_compile_options(
  -ffreestanding
  -nostdinc++
  -isystem ${CMAKE_CXX_COMPILER_RESOURCE_DIR}/include
)

option(INTEGRATION_TEST "Build with integration test suite linked in" OFF)

if(INTEGRATION_TEST)
  add_compile_definitions(INTEGRATION_TEST)
endif()

# add modules
add_subdirectory(core)
add_subdirectory(drivers)
add_subdirectory(lib)

if(INTEGRATION_TEST)
  add_subdirectory(tests/integration/uart)
  add_subdirectory(tests/integration/exceptions)
  add_subdirectory(tests/integration/pmm)
  add_subdirectory(tests/integration/mmu)
  add_subdirectory(tests/integration/guestMmu)
  add_subdirectory(tests/integration/bootpkg)
  add_subdirectory(tests/integration/vcpu)
endif()

add_executable(${ELF}
  ${BOOT_S}
  ${CMAKE_SOURCE_DIR}/arch/aarch64/vectors.S
  ${CMAKE_SOURCE_DIR}/arch/aarch64/exceptions.S
  ${CMAKE_SOURCE_DIR}/arch/aarch64/vcpu.S
)

if(INTEGRATION_TEST)
  target_sources(${ELF} PRIVATE
    tests/integration/suite.cpp
  )

  target_link_libraries(${ELF} PRIVATE
    -Wl,--whole-archive
    TestExceptions
    TestUart
    TestPmm
    TestMmu
    TestGuestMmu
    TestBootPkg
    TestVcpu
    -Wl,--no-whole-archive
  )
endif()

# link libs
target_link_libraries(${ELF}
  PRIVATE
  Core
  Uart
  Exceptions
  Pmm
  Heap
  PageTable
  HostMmu
  GuestMmu
  Dtb
  BootPkg
  Vcpu
  Vm
  Panic
  Strings
)

target_compile_options(${ELF}
  PRIVATE
  -ffreestanding
  -fno-exceptions
  -fno-rtti
  -fno-threadsafe-statics
  -fno-common
  -fno-builtin
  -fno-asynchronous-unwind-tables
  -ffunction-sections
  -fdata-sections
  -Wall
  -Wextra
  $<$<CONFIG:Debug>:-O0 -g3 -gdwarf-4>
  $<$<CONFIG:Release>:-Os -DNDEBUG>
)

target_link_options(${ELF}
  PRIVATE
  -T${LINKER_SCRIPT}
  -nostdlib
  -static
)

if(CMAKE_HOST_APPLE)
  target_link_options(${ELF} PRIVATE -fuse-ld=lld)
endif()

set_target_properties(${ELF}
  PROPERTIES
  LINK_DEPENDS ${LINKER_SCRIPT}
)

add_custom_command(
  TARGET ${ELF} POST_BUILD
  COMMAND ${CMAKE_OBJCOPY} -O binary
    $<TARGET_FILE:${ELF}>
    ${CMAKE_BINARY_DIR}/kernel8.img
    COMMENT "[LOG] Generating Kernel Image"
)

# QEMU run target
if (BOARD STREQUAL "qemu")
  set(QEMU_GUEST_KERNEL "${CMAKE_SOURCE_DIR}/Image"
      CACHE FILEPATH "Guest Linux kernel Image for QEMU")
  set(QEMU_GUEST_DTB    "${CMAKE_SOURCE_DIR}/boot/guest-qemu.dtb"
      CACHE FILEPATH "Guest DTB for QEMU")
  set(QEMU_GUEST_PACKAGE "${CMAKE_SOURCE_DIR}/boot/guest-qemu.hvgbp"
      CACHE STRING "QEMU guest boot package")

  find_program(CARGO_CMD cargo REQUIRED)

  add_custom_command(
    OUTPUT  ${QEMU_GUEST_PACKAGE}
    COMMAND ${CARGO_CMD} build --release --manifest-path
              ${CMAKE_SOURCE_DIR}/tools/mkguestpkg/Cargo.toml
    COMMAND ${CMAKE_SOURCE_DIR}/tools/mkguestpkg/target/release/mkguestpkg
              --kernel ${QEMU_GUEST_KERNEL}
              --dtb    ${QEMU_GUEST_DTB}
              --out    ${QEMU_GUEST_PACKAGE}
    DEPENDS ${QEMU_GUEST_KERNEL} ${QEMU_GUEST_DTB}
    COMMENT "[LOG] Building guest-qemu.hvgbp"
  )

  add_custom_target(guest_pkg DEPENDS ${QEMU_GUEST_PACKAGE})

  add_custom_target(run
    COMMAND qemu-system-aarch64
      -machine virt,virtualization=on,gic-version=2
      -cpu cortex-a76
      -m 4G
      -nographic
      -kernel ${CMAKE_BINARY_DIR}/kernel8.img
      -initrd ${QEMU_GUEST_PACKAGE}
    DEPENDS hyperberry.elf guest_pkg
    COMMENT "[LOG] Launching HyperBerry in QEMU"
  )
endif()

if (BOARD STREQUAL "rpi5")
  set(SD_MOUNT "/mnt/sdcard" CACHE STRING "SD card mount point")

  add_custom_target(run
    COMMAND ${CMAKE_COMMAND} -E copy
      ${CMAKE_BINARY_DIR}/kernel8.img
      ${SD_MOUNT}/kernel8.img
    COMMAND ${CMAKE_COMMAND} -E copy
      ${CMAKE_SOURCE_DIR}/boot/start4.elf
      ${SD_MOUNT}/start4.elf
    COMMAND ${CMAKE_COMMAND} -E copy
      ${CMAKE_SOURCE_DIR}/boot/bcm2712-rpi-5-b.dtb
      ${SD_MOUNT}/bcm2712-rpi-5-b.dtb
    COMMAND ${CMAKE_COMMAND} -E copy
      ${CMAKE_SOURCE_DIR}/boot/config.txt
      ${SD_MOUNT}/config.txt
    COMMAND ${CMAKE_COMMAND} -E copy
      ${CMAKE_SOURCE_DIR}/boot/fixup4.dat
      ${SD_MOUNT}/fixup4.dat
    COMMAND ${CMAKE_COMMAND} -E copy
      ${CMAKE_SOURCE_DIR}/boot/guest.hvgbp
      ${SD_MOUNT}/guest.hvgbp

    DEPENDS hyperberry.elf
    COMMENT "[LOG] Flashing the SD card for the RPI5 firmware"
  )
endif()
