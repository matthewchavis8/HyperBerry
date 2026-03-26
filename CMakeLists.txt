cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) 
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_SOURCE_DIR}/cmake/aarch64-toolchain.cmake CACHE STRING "Toolchain file")

set(CMAKE_OBJCOPY llvm-objcopy)
set(ELF hyperberry.elf)

project(HyperBerry VERSION 0.0.1 LANGUAGES CXX ASM C)

# Build option for targets
set(BOARD "rpi5" CACHE STRING "Target Board [rpi5 | qemu]")

if (BOARD STREQUAL "rpi5")
  message(STATUS "Building for target: Raspberry Pi 5")
  set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/linker/rpi5/linker.ld)
  set(BOOT_S        ${CMAKE_SOURCE_DIR}/arch/aarch64/rpi5/boot.S)
elseif (BOARD STREQUAL "qemu")
  message(STATUS "Building for target: Virtualized Raspberry Pi 5 in QEMU")
  set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/linker/qemu/linker.ld)
  set(BOOT_S        ${CMAKE_SOURCE_DIR}/arch/aarch64/qemu/boot.S)
  add_compile_definitions(QEMU)
else ()
  message(STATUS "[ERROR] unknown board detected={BOARD}")
endif()

# add modules
add_subdirectory(core)    # subject to change once we get code into core/
add_subdirectory(drivers)

add_executable(${ELF} ${BOOT_S})

# link libs
target_link_libraries(${ELF}
  PRIVATE
  Core
  Uart
)

target_link_options(${ELF}
  PRIVATE
  -T${LINKER_SCRIPT}
  -nostdlib
  -static
)

set_target_properties(${ELF}
  PROPERTIES
  LINK_DEPENDS ${LINKER_SCRIPT}
)

add_custom_command(
  TARGET ${ELF} POST_BUILD
  COMMAND llvm-objcopy -O binary
    $<TARGET_FILE:${ELF}>
    ${CMAKE_BINARY_DIR}/kernel8.img
    COMMENT "[LOG] Generating Kernel Image"
)

# QEMU run target
if (BOARD STREQUAL "qemu")
  add_custom_target(run
    COMMAND qemu-system-aarch64
      -machine virt,virtualization=on,gic-version=2
      -cpu cortex-a76
      -m 4G
      -nographic
      -kernel ${CMAKE_BINARY_DIR}/kernel8.img
    DEPENDS hyperberry.elf
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

    DEPENDS hyperberry.elf
    COMMENT "[LOG] Flashing the SD card for the RPI5 firmware"
  )
endif()

# Pull in Google Test
# COMMENT FOR NOW need to seperate debug and release later
# include(CTest)
# if (BUILD_TESTING)
#   # For MSVC compilers avoid conflict
#   set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
#
#   include(FetchContent)
#   FetchContent_Declare(
#     googletest
#     URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
#   )
#   FetchContent_MakeAvailable(googletest)
#
#   add_subdirectory(tests)
# endif()
