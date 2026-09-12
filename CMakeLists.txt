cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) 
set(CMAKE_OBJCOPY llvm-objcopy)
set(ELF hyperberry.elf)

project(HyperBerry VERSION 0.0.1 LANGUAGES CXX ASM C)

# Generate a board's assembler-safe address defines from its host device tree.
function(hb_generate_regs board dtb outdir)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  execute_process(
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/bspgen/bspgen.py
            --dtb ${dtb} --board ${board} --out ${outdir}/regs.inc
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE out
    ERROR_VARIABLE  out
  )
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "bspgen failed for ${board}: ${out}")
  endif()
  string(STRIP "${out}" out)
  message(STATUS "  ${out}")
endfunction()

set(HOST_DTB_rpi5 ${CMAKE_SOURCE_DIR}/bsp/rpi5/firmware/bcm2712-rpi-5-b.dtb)
set(HOST_DTB_qemu ${CMAKE_SOURCE_DIR}/bsp/qemu/dts/host-qemu.dtb)
set(HOST_DTB_fvp  ${CMAKE_SOURCE_DIR}/bsp/fvp/dts/host-fvp.dtb)

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

  set(UNIT_BSP_GEN ${CMAKE_BINARY_DIR}/qemu/generated)
  hb_generate_regs(qemu ${HOST_DTB_qemu} ${UNIT_BSP_GEN})

  add_subdirectory(tests/unit)
  return()
endif()


# One configure builds every board. Each produces its own image under
# ${CMAKE_BINARY_DIR}/<board>/; flashing picks the one it needs.
set(SUPPORTED_BOARDS rpi5 qemu fvp)
set(BOARD_DESC_rpi5 "Raspberry Pi 5")
set(BOARD_DESC_qemu "Virtualized Raspberry Pi 5 in QEMU")
set(BOARD_DESC_fvp  "Arm FVP Base RevC-2xAEMvA")

option(BUILD_INTEGRATION "Also build the per-board integration images" ON)


include_directories(${CMAKE_SOURCE_DIR})

add_compile_options(
  -ffreestanding
  -fno-exceptions
  -fno-rtti
  -fno-threadsafe-statics
  -fno-asynchronous-unwind-tables
  -nostdinc++
  -isystem ${CMAKE_CXX_COMPILER_RESOURCE_DIR}/include
)

# Linked into the image rather than into Virt: the vector table is the only
# thing that references the entry points, so as archive members they would be
# dropped before KEEP in the linker script ever applied.
set(HB_ENTRY_SOURCES
  ${CMAKE_SOURCE_DIR}/core/exceptions/vectors.S
  ${CMAKE_SOURCE_DIR}/core/exceptions/exceptions.S
  ${CMAKE_SOURCE_DIR}/core/vcpu/vcpu.S
)

# Subsystem libraries, in layering order: each may depend only on those below.
set(HB_SUBSYSTEMS Virt Mm Drivers Lib)

# Build one hypervisor image. `core` is the core variant to link (Core or
# CoreTest); `outdir` receives kernel8.img.
function(hb_add_image name core outdir)
  add_executable(${name} ${BSP_DIR}/boot.S ${HB_ENTRY_SOURCES})

  target_include_directories(${name} PRIVATE ${CMAKE_SOURCE_DIR} ${BSP_DIR} ${BSP_GEN})

  if(core STREQUAL "CoreTest")
    target_sources(${name} PRIVATE ${CMAKE_SOURCE_DIR}/tests/integration/suite.cpp)
    target_compile_definitions(${name} PRIVATE INTEGRATION_TEST)

    # Test suites self-register, so nothing references them at link time.
    target_link_libraries(${name} PRIVATE
      -Wl,--whole-archive Tests${BSP_SUFFIX} -Wl,--no-whole-archive)
  endif()

  set(libs ${core}${BSP_SUFFIX})
  foreach(m IN LISTS HB_SUBSYSTEMS)
    list(APPEND libs ${m}${BSP_SUFFIX})
  endforeach()
  target_link_libraries(${name} PRIVATE ${libs})

  target_compile_options(${name} PRIVATE
    -fno-common
    -fno-builtin
    -ffunction-sections
    -fdata-sections
    -Wall
    -Wextra
    $<$<CONFIG:Debug>:-O0 -g3 -gdwarf-4>
    $<$<CONFIG:Release>:-Os -DNDEBUG>
  )

  # Makes the -ffunction-sections/-fdata-sections above mean something. It
  # currently reclaims nothing -- only the empty .text left over from boot.S
  # and vectors.S, whose real code sits in KEEPt sections -- but keeps dead
  # code from accumulating silently. The linker scripts KEEP the entry,
  # vectors and self-registering test section, the only inputs nothing
  # references by symbol.
  target_link_options(${name} PRIVATE
    -T${BSP_DIR}/linker.ld
    -nostdlib
    -static
    -Wl,--gc-sections
  )
  if(CMAKE_HOST_APPLE)
    target_link_options(${name} PRIVATE -fuse-ld=lld)
  endif()

  set_target_properties(${name} PROPERTIES
    LINK_DEPENDS             ${BSP_DIR}/linker.ld
    RUNTIME_OUTPUT_DIRECTORY ${outdir}
  )

  add_custom_command(TARGET ${name} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${name}> ${outdir}/kernel8.img
    COMMENT "[LOG] Generating ${outdir}/kernel8.img"
  )
endfunction()

foreach(board IN LISTS SUPPORTED_BOARDS)
  message(STATUS "Board: ${board} (${BOARD_DESC_${board}})")

  set(BSP_BOARD  ${board})
  set(BSP_SUFFIX -${board})
  set(BSP_DIR    ${CMAKE_SOURCE_DIR}/bsp/${board})
  set(BOARD_OUT  ${CMAKE_BINARY_DIR}/${board})
  set(BSP_GEN    ${CMAKE_BINARY_DIR}/${board}/generated)

  hb_generate_regs(${board} ${HOST_DTB_${board}} ${BSP_GEN})

  # bsp/<board> and its generated dir on the include path is what selects the
  # board's constants. There is no board macro.
  set_directory_properties(PROPERTIES
    INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR};${BSP_DIR};${BSP_GEN}")

  add_subdirectory(lib     lib-${board})
  add_subdirectory(drivers drivers-${board})
  add_subdirectory(core    core-${board})

  hb_add_image(hyperberry-${board} Core ${BOARD_OUT})

  if(BUILD_INTEGRATION)
    add_subdirectory(tests/integration tests-${board})
    hb_add_image(hyperberry-${board}-test CoreTest ${BOARD_OUT}/integration)
  endif()
endforeach()

# ---------------------------------------------------------------------------
# Run / flash targets. Host tools are looked up without REQUIRED so a missing
# dtc or cargo disables one board's run target instead of failing the build.
# ---------------------------------------------------------------------------
find_program(CARGO_CMD cargo)
find_program(DTC       dtc)
find_program(FDTPUT    fdtput)

set(MKGUESTPKG ${CMAKE_SOURCE_DIR}/tools/mkguestpkg/target/release/mkguestpkg)

# --- qemu -------------------------------------------------------------------
set(QEMU_GUEST_KERNEL  "${CMAKE_SOURCE_DIR}/Image"
    CACHE FILEPATH "Guest Linux kernel Image for QEMU")
set(QEMU_GUEST_DTS     "${CMAKE_SOURCE_DIR}/bsp/qemu/dts/guest-qemu.dts")
set(QEMU_GUEST_DTB     "${CMAKE_BINARY_DIR}/qemu/guest-qemu.dtb")
set(QEMU_GUEST_PACKAGE "${CMAKE_SOURCE_DIR}/boot/profiles/guest-qemu.hvgbp"
    CACHE STRING "QEMU guest boot package")

if(CARGO_CMD AND DTC)
  add_custom_command(
    OUTPUT ${QEMU_GUEST_DTB}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/qemu
    COMMAND ${DTC} -I dts -O dtb -i ${CMAKE_SOURCE_DIR}/boot/dts
            -o ${QEMU_GUEST_DTB} ${QEMU_GUEST_DTS}
    DEPENDS ${QEMU_GUEST_DTS} ${CMAKE_SOURCE_DIR}/boot/dts/guest-base.dtsi
    COMMENT "[LOG] Compiling guest-qemu.dtb"
  )

  add_custom_command(
    OUTPUT  ${QEMU_GUEST_PACKAGE}
    COMMAND ${CARGO_CMD} build --release --manifest-path
              ${CMAKE_SOURCE_DIR}/tools/mkguestpkg/Cargo.toml
    COMMAND ${MKGUESTPKG}
              --kernel ${QEMU_GUEST_KERNEL}
              --dtb    ${QEMU_GUEST_DTB}
              --out    ${QEMU_GUEST_PACKAGE}
    DEPENDS ${QEMU_GUEST_KERNEL} ${QEMU_GUEST_DTB}
    COMMENT "[LOG] Building guest-qemu.hvgbp"
  )
  add_custom_target(guest_pkg DEPENDS ${QEMU_GUEST_PACKAGE})

  add_custom_target(run-qemu
    COMMAND qemu-system-aarch64
      -machine virt,virtualization=on,gic-version=2
      -cpu cortex-a76
      -m 8G
      -nographic
      -kernel ${CMAKE_BINARY_DIR}/qemu/kernel8.img
      -initrd ${QEMU_GUEST_PACKAGE}
    DEPENDS hyperberry-qemu guest_pkg
    COMMENT "[LOG] Launching HyperBerry in QEMU"
  )

  if(BUILD_INTEGRATION)
    add_custom_target(run-qemu-test
      COMMAND qemu-system-aarch64
        -machine virt,virtualization=on,gic-version=2
        -cpu cortex-a76
        -m 8G
        -nographic
        -kernel ${CMAKE_BINARY_DIR}/qemu/integration/kernel8.img
        -initrd ${QEMU_GUEST_PACKAGE}
      DEPENDS hyperberry-qemu-test guest_pkg
      COMMENT "[LOG] Running HyperBerry integration suite in QEMU"
    )
  endif()
else()
  message(STATUS "cargo/dtc not both found - run-qemu disabled")
endif()

# --- rpi5 -------------------------------------------------------------------
set(SD_MOUNT "/mnt/sdcard" CACHE STRING "SD card mount point")
set(RPI5_FW  ${CMAKE_SOURCE_DIR}/bsp/rpi5/firmware)

add_custom_target(flash-rpi5
  COMMAND ${CMAKE_COMMAND} -E copy
    ${CMAKE_BINARY_DIR}/rpi5/kernel8.img       ${SD_MOUNT}/kernel8.img
  COMMAND ${CMAKE_COMMAND} -E copy
    ${RPI5_FW}/start4.elf                      ${SD_MOUNT}/start4.elf
  COMMAND ${CMAKE_COMMAND} -E copy
    ${RPI5_FW}/bcm2712-rpi-5-b.dtb             ${SD_MOUNT}/bcm2712-rpi-5-b.dtb
  COMMAND ${CMAKE_COMMAND} -E copy
    ${RPI5_FW}/config.txt                      ${SD_MOUNT}/config.txt
  COMMAND ${CMAKE_COMMAND} -E copy
    ${RPI5_FW}/fixup4.dat                      ${SD_MOUNT}/fixup4.dat
  COMMAND ${CMAKE_COMMAND} -E copy
    ${CMAKE_SOURCE_DIR}/boot/profiles/guest.hvgbp ${SD_MOUNT}/guest.hvgbp
  DEPENDS hyperberry-rpi5
  COMMENT "[LOG] Flashing the SD card for the RPI5 firmware"
)

# --- fvp --------------------------------------------------------------------
set(FVP_BIN "${CMAKE_SOURCE_DIR}/tools/FVP/bin/FVP_Base_RevC-2xAEMvA"
    CACHE FILEPATH "Path to the FVP Base RevC binary")

if(DTC AND FDTPUT AND CARGO_CMD)
  set(FVP_DIR           ${CMAKE_SOURCE_DIR}/bsp/fvp)

  # Single-source the load addresses with boot.S.
  file(STRINGS ${FVP_DIR}/platform.inc FVP_HOST_DTB_ADDR
       REGEX "define BSP_HOST_DTB_BASE")
  file(STRINGS ${FVP_DIR}/platform.inc FVP_PKG_ADDR
       REGEX "define BSP_GUEST_PACKAGE_BASE")
  string(REGEX REPLACE ".*(0x[0-9a-fA-F]+).*" "\\1" FVP_HOST_DTB_ADDR "${FVP_HOST_DTB_ADDR}")
  string(REGEX REPLACE ".*(0x[0-9a-fA-F]+).*" "\\1" FVP_PKG_ADDR "${FVP_PKG_ADDR}")
  set(FVP_HOST_DTS      "${FVP_DIR}/dts/host-fvp.dts")
  set(FVP_HOST_DTB      "${CMAKE_BINARY_DIR}/fvp/host-fvp.dtb")
  set(FVP_GUEST_DTS     "${FVP_DIR}/dts/guest-fvp.dts")
  set(FVP_GUEST_DTB     "${CMAKE_BINARY_DIR}/fvp/guest-fvp.dtb")
  set(FVP_GUEST_KERNEL  "${CMAKE_SOURCE_DIR}/Image"
      CACHE FILEPATH "Guest Linux kernel Image for FVP")
  set(FVP_GUEST_PACKAGE "${CMAKE_BINARY_DIR}/fvp/host-fvp.hvgbp")

  add_custom_command(
    OUTPUT ${FVP_GUEST_DTB}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/fvp
    COMMAND ${DTC} -I dts -O dtb -i ${CMAKE_SOURCE_DIR}/boot/dts
            -o ${FVP_GUEST_DTB} ${FVP_GUEST_DTS}
    DEPENDS ${FVP_GUEST_DTS} ${CMAKE_SOURCE_DIR}/boot/dts/guest-base.dtsi
    COMMENT "[LOG] Compiling guest-fvp.dtb"
  )

  add_custom_command(
    OUTPUT  ${FVP_GUEST_PACKAGE}
    COMMAND ${CARGO_CMD} build --release --manifest-path
              ${CMAKE_SOURCE_DIR}/tools/mkguestpkg/Cargo.toml
    COMMAND ${MKGUESTPKG}
              --kernel ${FVP_GUEST_KERNEL}
              --dtb    ${FVP_GUEST_DTB}
              --out    ${FVP_GUEST_PACKAGE}
    DEPENDS ${FVP_GUEST_KERNEL} ${FVP_GUEST_DTB}
    COMMENT "[LOG] Building host-fvp.hvgbp"
  )

  add_custom_command(
    OUTPUT ${FVP_HOST_DTB}
    COMMAND ${DTC} -I dts -O dtb -o ${FVP_HOST_DTB} ${FVP_HOST_DTS}
    COMMAND python3 -c
      "import os,sys,subprocess; sz=os.path.getsize(sys.argv[1]); subprocess.check_call([sys.argv[2],'-tx',sys.argv[3],'/chosen','linux,initrd-start','0x0',sys.argv[4]]); subprocess.check_call([sys.argv[2],'-tx',sys.argv[3],'/chosen','linux,initrd-end','0x0',hex(int(sys.argv[4],16)+sz)])"
      ${FVP_GUEST_PACKAGE} ${FDTPUT} ${FVP_HOST_DTB} ${FVP_PKG_ADDR}
    DEPENDS ${FVP_HOST_DTS} ${FVP_GUEST_PACKAGE}
    COMMENT "[LOG] Compiling host-fvp.dtb and patching /chosen initrd range"
    VERBATIM
  )

  add_custom_target(fvp_host_dtb DEPENDS ${FVP_HOST_DTB})
  add_dependencies(hyperberry-fvp fvp_host_dtb)

  add_custom_target(run-fvp
    COMMAND ${FVP_BIN}
      -a $<TARGET_FILE:hyperberry-fvp>
      --data ${FVP_HOST_DTB}@${FVP_HOST_DTB_ADDR}
      --data ${FVP_GUEST_PACKAGE}@${FVP_PKG_ADDR}
      -C bp.secure_memory=0
      -C cluster0.NUM_CORES=1
      -C cluster1.NUM_CORES=0
      -C bp.vis.disable_visualisation=1
      -C bp.terminal_0.start_telnet=0
      -C bp.pl011_uart0.out_file=-
      -C bp.pl011_uart0.unbuffered_output=1
      -C cache_state_modelled=0
    DEPENDS hyperberry-fvp fvp_host_dtb ${FVP_GUEST_PACKAGE}
    COMMENT "[LOG] Launching HyperBerry on Arm FVP Base RevC"
  )
else()
  message(STATUS "dtc/fdtput/cargo not all found - run-fvp disabled")
endif()
