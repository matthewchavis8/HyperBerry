add_subdirectory(mm)

# Guest execution: vCPU state, VM lifecycle, and the EL2 trap and HVC path.
add_library(Virt${BSP_SUFFIX}
  STATIC
  vcpu/vcpu.cpp
  vm/vm.cpp
  vmm/vmm.cpp
  vmm/hvc/hvc.cpp
)

target_include_directories(Virt${BSP_SUFFIX}
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/vcpu
  ${CMAKE_CURRENT_SOURCE_DIR}/vm
  ${CMAKE_CURRENT_SOURCE_DIR}/vmm
  ${CMAKE_CURRENT_SOURCE_DIR}/vmm/hvc
)

target_link_libraries(Virt${BSP_SUFFIX}
  PUBLIC
  Mm${BSP_SUFFIX}
  Drivers${BSP_SUFFIX}
  Lib${BSP_SUFFIX}
)

# vcpu.S indexes into Vcpu through offsets the compiler computes. The object
# library is compiled to assembly text rather than to an object, and the
# script lifts the offsets out of it into a header the .S files include.
find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_library(VcpuOffsets${BSP_SUFFIX} OBJECT vcpu/vcpuOffsets.cpp)
target_compile_options(VcpuOffsets${BSP_SUFFIX} PRIVATE -S -Wno-unused-command-line-argument)

set(HB_VCPU_OFFSETS ${BSP_GEN}/vcpuOffsets.h)
add_custom_command(
  OUTPUT ${HB_VCPU_OFFSETS}
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/asmoffsets/asmoffsets.py
          --in $<TARGET_OBJECTS:VcpuOffsets${BSP_SUFFIX}> --out ${HB_VCPU_OFFSETS}
  DEPENDS VcpuOffsets${BSP_SUFFIX}
          $<TARGET_OBJECTS:VcpuOffsets${BSP_SUFFIX}>
          ${CMAKE_SOURCE_DIR}/scripts/asmoffsets/asmoffsets.py
  VERBATIM
)
add_custom_target(VcpuOffsetsHeader${BSP_SUFFIX} DEPENDS ${HB_VCPU_OFFSETS})

set(HB_CORE_SOURCES
  main.cpp
  deviceTree/deviceTree.cpp
  bootLoader/bootLoader.cpp
)

set(HB_CORE_DEPS
  Virt${BSP_SUFFIX}
  Mm${BSP_SUFFIX}
  Drivers${BSP_SUFFIX}
  Lib${BSP_SUFFIX}
)

add_library(Core${BSP_SUFFIX}
  STATIC
  ${HB_CORE_SOURCES}
)

target_include_directories(Core${BSP_SUFFIX}
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/deviceTree
  ${CMAKE_CURRENT_SOURCE_DIR}/bootLoader
)

target_link_libraries(Core${BSP_SUFFIX}
  PRIVATE
  ${HB_CORE_DEPS}
)

# INTEGRATION_TEST only changes main.cpp, so the test build reuses every other
# subsystem and swaps in this variant.
if(BUILD_INTEGRATION)
  add_library(CoreTest${BSP_SUFFIX}
    STATIC
    ${HB_CORE_SOURCES}
  )

  target_compile_definitions(CoreTest${BSP_SUFFIX}
    PRIVATE
    INTEGRATION_TEST
  )

  target_include_directories(CoreTest${BSP_SUFFIX}
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/deviceTree
    ${CMAKE_CURRENT_SOURCE_DIR}/bootLoader
  )

  target_link_libraries(CoreTest${BSP_SUFFIX}
    PRIVATE
    ${HB_CORE_DEPS}
  )
endif()
