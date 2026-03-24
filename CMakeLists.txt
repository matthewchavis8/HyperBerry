cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) 
set(CMAKE_OBJCOPY llvm-objcopy)
set(ELF hyperberry.elf)
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_SOURCE_DIR}/cmake/aarch64-toolchain.cmake CACHE STRING "Toolchain file")
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
else ()
  message(STATUS "[ERROR] unknown board detected={BOARD}")
endif()

# add modules
add_subdirectory(core)
add_executable(${ELF} ${BOOT_S})

# link libs
target_link_libraries(${ELF}
  PRIVATE
  Core
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
