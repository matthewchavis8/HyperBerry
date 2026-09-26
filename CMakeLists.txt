cmake_minimum_required(VERSION 3.25)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) 
find_program(HB_OBJCOPY NAMES llvm-objcopy-22 llvm-objcopy REQUIRED)
set(CMAKE_OBJCOPY ${HB_OBJCOPY})
set(ELF hyperberry.elf)

project(HyperBerry VERSION 0.0.1 LANGUAGES CXX ASM C)

if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_VERSION VERSION_LESS 22.1)
  message(FATAL_ERROR "HyperBerry requires LLVM Clang 22.1 or newer")
endif()

execute_process(
  COMMAND ${CMAKE_CXX_COMPILER} -print-resource-dir
  OUTPUT_VARIABLE HB_CLANG_RESOURCE_DIR
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

set(HB_DEFAULT_SYSROOT
    ${CMAKE_SOURCE_DIR}/.toolchain/arm-newlib-19.1.5/lib/clang-runtimes/newlib/aarch64-none-elf/aarch64a)
if(NOT EXISTS ${HB_DEFAULT_SYSROOT})
  set(HB_DEFAULT_SYSROOT "")
endif()

set(HB_LLVM_SYSROOT "" CACHE PATH
    "Arm newlib and libc++ sysroot for aarch64 none elf")
if(NOT "$ENV{HB_LLVM_SYSROOT}" STREQUAL "")
  set(HB_LLVM_SYSROOT "$ENV{HB_LLVM_SYSROOT}" CACHE PATH
      "Arm newlib and libc++ sysroot for aarch64 none elf" FORCE)
elseif(HB_LLVM_SYSROOT STREQUAL "" AND NOT HB_DEFAULT_SYSROOT STREQUAL "")
  set(HB_LLVM_SYSROOT ${HB_DEFAULT_SYSROOT} CACHE PATH
      "Arm newlib and libc++ sysroot for aarch64 none elf" FORCE)
endif()

if(NOT BUILD_TESTING)
  if(HB_LLVM_SYSROOT STREQUAL "")
    message(FATAL_ERROR
      "HB_LLVM_SYSROOT must name the Arm newlib and libc++ sysroot. Run scripts/toolchain/fetch_sysroot.py first.")
  endif()

  if(NOT CMAKE_CXX_COMPILER_TARGET STREQUAL "aarch64-none-elf")
    message(FATAL_ERROR "The bare metal compiler target must be aarch64-none-elf")
  endif()

  set(HB_LIBCXX_INCLUDE ${HB_LLVM_SYSROOT}/include/c++/v1)
  set(HB_LIBC_INCLUDE ${HB_LLVM_SYSROOT}/include)
  set(HB_LIBC_LIBDIR ${HB_LLVM_SYSROOT}/lib)
  set(HB_STDLIB_ARCHIVES
    ${HB_LIBC_LIBDIR}/libc++.a
    ${HB_LIBC_LIBDIR}/libc++abi.a
    ${HB_LIBC_LIBDIR}/libunwind.a
    ${HB_LIBC_LIBDIR}/libc.a
    ${HB_LIBC_LIBDIR}/libclang_rt.builtins.a
  )
  foreach(path ${HB_LIBCXX_INCLUDE} ${HB_LIBC_INCLUDE} ${HB_STDLIB_ARCHIVES})
    if(NOT EXISTS ${path})
      message(FATAL_ERROR "HB_LLVM_SYSROOT is missing ${path}")
    endif()
  endforeach()
endif()

# Generate a board's assembler-safe address defines from its host device tree.
function(hb_generate_regs board dtb outdir)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  execute_process(
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/bspgen/bspgen.py
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
set(SUPPORTED_BOARDS rpi5 qemu)
set(BOARD_DESC_rpi5 "Raspberry Pi 5")
set(BOARD_DESC_qemu "Virtualized Raspberry Pi 5 in QEMU")

option(BUILD_INTEGRATION "Also build the per-board integration images" ON)


include_directories(${CMAKE_SOURCE_DIR})

add_compile_options(
  -ffreestanding
  -fno-exceptions
  -fno-rtti
  -fno-threadsafe-statics
  -fno-asynchronous-unwind-tables
  -nostdinc++
  -isystem${HB_LIBCXX_INCLUDE}
  -isystem${HB_LIBC_INCLUDE}
  -isystem${HB_CLANG_RESOURCE_DIR}/include
)

# Linked into the image rather than into Virt: the vector table is the only
# thing that references the entry points, so as archive members they would be
# dropped before KEEP in the linker script ever applied.
set(HB_ENTRY_SOURCES
  ${CMAKE_SOURCE_DIR}/core/vmm/vectors.S
  ${CMAKE_SOURCE_DIR}/core/vmm/vmm.S
  ${CMAKE_SOURCE_DIR}/core/vcpu/vcpu.S
)

# Subsystem libraries, in layering order: each may depend only on those below.
set(HB_SUBSYSTEMS Virt Mm Drivers Lib)

# Build one hypervisor image. `core` is the core variant to link (Core or
# CoreTest); `outdir` receives kernel8.img.
function(hb_add_image name core outdir)
  add_executable(${name} ${BSP_DIR}/boot.S ${HB_ENTRY_SOURCES})
  add_dependencies(${name} VcpuOffsetsHeader${BSP_SUFFIX})

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
  target_link_libraries(${name} PRIVATE
    -Wl,--start-group ${libs} ${HB_STDLIB_ARCHIVES} -Wl,--end-group)

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
  target_link_options(${name} PRIVATE -fuse-ld=lld)

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


find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_program(CPIO cpio REQUIRED)
find_program(DTC dtc REQUIRED)
find_program(FDTPUT fdtput)
set(ARCHIVE_TOOL ${CMAKE_SOURCE_DIR}/scripts/cpio/archive.py)
set(BUSYBOX_FETCH_TOOL ${CMAKE_SOURCE_DIR}/scripts/busybox/fetch.py)
set(BUSYBOX_ROOTFS_TOOL ${CMAKE_SOURCE_DIR}/scripts/busybox/rootfs.py)
set(QEMU_KERNEL_FETCH_TOOL ${CMAKE_SOURCE_DIR}/scripts/linux/fetch.py)
set(BUSYBOX_BINARY ${CMAKE_BINARY_DIR}/busybox/busybox)
set(BUSYBOX_ROOTFS ${CMAKE_BINARY_DIR}/busybox/rootfs.cpio)
set(QEMU_DEFAULT_GUEST_KERNEL ${CMAKE_BINARY_DIR}/linux/qemu/Image)
set(QEMU_GUEST_KERNEL ${QEMU_DEFAULT_GUEST_KERNEL} CACHE FILEPATH "Linux Image for qemu")

add_custom_command(
  OUTPUT ${QEMU_DEFAULT_GUEST_KERNEL}
  COMMAND ${Python3_EXECUTABLE} ${QEMU_KERNEL_FETCH_TOOL} --out ${QEMU_DEFAULT_GUEST_KERNEL}
  DEPENDS ${QEMU_KERNEL_FETCH_TOOL}
  VERBATIM
)

add_custom_command(
  OUTPUT ${BUSYBOX_BINARY}
  COMMAND ${Python3_EXECUTABLE} ${BUSYBOX_FETCH_TOOL}
          --cache ${CMAKE_BINARY_DIR}/busybox
          --out ${BUSYBOX_BINARY}
  DEPENDS ${BUSYBOX_FETCH_TOOL}
  VERBATIM
)

add_custom_command(
  OUTPUT ${BUSYBOX_ROOTFS}
  COMMAND ${Python3_EXECUTABLE} ${BUSYBOX_ROOTFS_TOOL}
          --busybox ${BUSYBOX_BINARY} --out ${BUSYBOX_ROOTFS}
  DEPENDS ${BUSYBOX_BINARY} ${BUSYBOX_ROOTFS_TOOL}
  VERBATIM
)
add_custom_target(busybox-rootfs DEPENDS ${BUSYBOX_ROOTFS})

if(BUILD_INTEGRATION)
  foreach(payload vcpu gic)
    set(payload_dir ${CMAKE_BINARY_DIR}/payloads)
    add_custom_command(
      OUTPUT ${payload_dir}/${payload}.bin
      COMMAND ${CMAKE_COMMAND} -E make_directory ${payload_dir}
      COMMAND ${CMAKE_ASM_COMPILER} --target=aarch64-none-elf -fuse-ld=lld
              -nostdlib -Wl,--build-id=none
              -Wl,-T,${CMAKE_SOURCE_DIR}/tests/integration/guest/payload.ld
              ${CMAKE_SOURCE_DIR}/tests/integration/${payload}/guest_payload.S
              -o ${payload_dir}/${payload}.elf
      COMMAND ${CMAKE_OBJCOPY} -O binary ${payload_dir}/${payload}.elf
              ${payload_dir}/${payload}.bin
      DEPENDS tests/integration/${payload}/guest_payload.S
              tests/integration/guest/payload.ld
      BYPRODUCTS ${payload_dir}/${payload}.elf
      VERBATIM
    )
  endforeach()
  add_custom_target(test-payloads DEPENDS ${payload_dir}/vcpu.bin ${payload_dir}/gic.bin)
endif()

foreach(board IN LISTS SUPPORTED_BOARDS)
  string(TOUPPER ${board} upper)
  if(board STREQUAL qemu)
    set(default_guest_kernel ${QEMU_DEFAULT_GUEST_KERNEL})
  else()
    set(default_guest_kernel ${CMAKE_SOURCE_DIR}/Image)
  endif()
  set(${upper}_GUEST_KERNEL "${default_guest_kernel}" CACHE FILEPATH "Linux Image for ${board}")
  set(${upper}_GUEST_INITRD "" CACHE FILEPATH "Linux initrd override for ${board}")
  set(guest_dts ${CMAKE_SOURCE_DIR}/bsp/${board}/dts/guest-${board}.dts)
  set(guest_dtb ${CMAKE_BINARY_DIR}/${board}/guest.dtb)
  add_custom_command(
    OUTPUT ${guest_dtb}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/${board}
    COMMAND ${DTC} -I dts -O dtb -i ${CMAKE_SOURCE_DIR}/boot/dts
            -o ${guest_dtb} ${guest_dts}
    DEPENDS ${guest_dts} ${CMAKE_SOURCE_DIR}/boot/dts/guest-linux.dtsi
    VERBATIM
  )
  set(archive_base_args --file linux/Image ${${upper}_GUEST_KERNEL}
                        --file linux/guest.dtb ${guest_dtb})
  set(archive_args ${archive_base_args})
  set(archive_base_deps ${${upper}_GUEST_KERNEL} ${guest_dtb} ${ARCHIVE_TOOL})
  set(archive_deps ${archive_base_deps})
  if(NOT "${${upper}_GUEST_INITRD}" STREQUAL "")
    list(APPEND archive_args --file linux/initrd ${${upper}_GUEST_INITRD})
    list(APPEND archive_deps ${${upper}_GUEST_INITRD})
  else()
    list(APPEND archive_args --file linux/initrd ${BUSYBOX_ROOTFS})
    list(APPEND archive_deps ${BUSYBOX_ROOTFS})
  endif()
  set(${upper}_GUEST_ARCHIVE ${CMAKE_BINARY_DIR}/${board}/guest.cpio)
  add_custom_command(
    OUTPUT ${${upper}_GUEST_ARCHIVE}
    COMMAND ${Python3_EXECUTABLE} ${ARCHIVE_TOOL}
            ${archive_args} --out ${${upper}_GUEST_ARCHIVE}
    DEPENDS ${archive_deps}
    VERBATIM
  )
  add_custom_target(guest-archive-${board} DEPENDS ${${upper}_GUEST_ARCHIVE})
  add_dependencies(hyperberry-${board} guest-archive-${board})
  if(BUILD_INTEGRATION)
    set(${upper}_TEST_ARCHIVE ${CMAKE_BINARY_DIR}/${board}/integration/guest.cpio)
    add_custom_command(
      OUTPUT ${${upper}_TEST_ARCHIVE}
      COMMAND ${Python3_EXECUTABLE} ${ARCHIVE_TOOL} ${archive_base_args}
              --file tests/vcpu.bin ${payload_dir}/vcpu.bin
              --file tests/gic.bin ${payload_dir}/gic.bin
              --out ${${upper}_TEST_ARCHIVE}
      DEPENDS ${archive_base_deps} ${payload_dir}/vcpu.bin ${payload_dir}/gic.bin
      VERBATIM
    )
    add_custom_target(test-archive-${board} DEPENDS ${${upper}_TEST_ARCHIVE})
    add_dependencies(test-archive-${board} test-payloads)
    add_dependencies(hyperberry-${board}-test test-archive-${board})
  endif()
endforeach()

add_custom_target(run-qemu
  COMMAND qemu-system-aarch64
    -machine virt,virtualization=on,gic-version=2 -cpu cortex-a76 -m 8G -nographic
    -kernel ${CMAKE_BINARY_DIR}/qemu/kernel8.img -initrd ${QEMU_GUEST_ARCHIVE}
  DEPENDS hyperberry-qemu guest-archive-qemu
  VERBATIM
)
if(BUILD_INTEGRATION)
  add_custom_target(run-qemu-test
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_SOURCE_DIR}/.github/actions/run-qemu-integration/run_qemu_integration.py
      --kernel ${CMAKE_BINARY_DIR}/qemu/integration/kernel8.img
      --initrd ${QEMU_TEST_ARCHIVE}
      --log ${CMAKE_BINARY_DIR}/qemu/integration/qemu.log
    DEPENDS hyperberry-qemu-test
    VERBATIM
  )
endif()

set(SD_MOUNT "/mnt/sdcard" CACHE STRING "SD card mount point")
set(RPI5_FW ${CMAKE_SOURCE_DIR}/bsp/rpi5/firmware)
function(add_flash_target target image archive)
  add_custom_target(${target}
    COMMAND ${CMAKE_COMMAND} -E copy ${image} ${SD_MOUNT}/kernel8.img
    COMMAND ${CMAKE_COMMAND} -E copy ${archive} ${SD_MOUNT}/guest.cpio
    COMMAND ${CMAKE_COMMAND} -E copy
      ${RPI5_FW}/start4.elf ${RPI5_FW}/bcm2712-rpi-5-b.dtb
      ${RPI5_FW}/config.txt ${RPI5_FW}/fixup4.dat ${SD_MOUNT}
    VERBATIM
  )
endfunction()
add_flash_target(flash-rpi5 ${CMAKE_BINARY_DIR}/rpi5/kernel8.img ${RPI5_GUEST_ARCHIVE})
add_dependencies(flash-rpi5 hyperberry-rpi5 guest-archive-rpi5)
if(BUILD_INTEGRATION)
  add_flash_target(flash-rpi5-test ${CMAKE_BINARY_DIR}/rpi5/integration/kernel8.img ${RPI5_TEST_ARCHIVE})
  add_dependencies(flash-rpi5-test hyperberry-rpi5-test)
endif()
