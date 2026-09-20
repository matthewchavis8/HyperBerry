add_executable(hyperberry_unit_tests
  pageTable/test_pageTable.cpp
  hostMmu/test_hostMmu.cpp
  guestMmu/test_guestMmu.cpp
  vcpu/test_vcpu.cpp
  vmm/test_esr.cpp
  vmm/test_hvc.cpp

  strings/test_strings.cpp
  ${CMAKE_SOURCE_DIR}/lib/strings/strings.cpp

  deviceTree/test_deviceTree.cpp
  log/test_log_format.cpp
  ${CMAKE_SOURCE_DIR}/lib/log/log.cpp
  gic/test_gic.cpp
  ${CMAKE_SOURCE_DIR}/core/deviceTree/deviceTree.cpp

  cpio/test_cpio.cpp
  ${CMAKE_SOURCE_DIR}/lib/cpio/cpio.cpp
  guest/test_guest.cpp
  ${CMAKE_SOURCE_DIR}/core/guest/guest.cpp

  vm/test_vm.cpp
  ${CMAKE_SOURCE_DIR}/core/vm/vm.cpp

  ${CMAKE_SOURCE_DIR}/core/vmm/hvc/hvc.cpp
)

target_include_directories(hyperberry_unit_tests
  PRIVATE
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/bsp/qemu
  ${CMAKE_BINARY_DIR}/qemu/generated
)

target_compile_options(hyperberry_unit_tests
  PRIVATE
  -fno-builtin-memcpy
  -fno-builtin-memset
)

target_link_libraries(hyperberry_unit_tests
  PRIVATE
  GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(hyperberry_unit_tests)

# Heap tests live in their own executable: heap.cpp's PMM stub conflicts
# with the sentinel-returning pmm stub used by the Vm unit tests.
add_executable(hyperberry_heap_tests
  heap/test_heap.cpp
  ${CMAKE_SOURCE_DIR}/core/mm/heap/heap.cpp
)

target_include_directories(hyperberry_heap_tests
  PRIVATE
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/bsp/qemu
  ${CMAKE_BINARY_DIR}/qemu/generated
)

target_compile_definitions(hyperberry_heap_tests
  PRIVATE
  HEAP_TESTING_BUILD
)

target_link_libraries(hyperberry_heap_tests
  PRIVATE
  GTest::gtest_main
)

gtest_discover_tests(hyperberry_heap_tests)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_program(CPIO cpio REQUIRED)
set(CPIO_FIXTURE ${CMAKE_CURRENT_BINARY_DIR}/fixture.cpio)
add_custom_command(
  OUTPUT ${CPIO_FIXTURE}
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/cpio/archive.py
          --root ${CMAKE_CURRENT_SOURCE_DIR}/cpio/root --out ${CPIO_FIXTURE}
  DEPENDS cpio/root/message.txt ${CMAKE_SOURCE_DIR}/tools/cpio/archive.py
  VERBATIM
)
add_custom_target(cpio_fixture DEPENDS ${CPIO_FIXTURE})
add_dependencies(hyperberry_unit_tests cpio_fixture)
target_compile_definitions(hyperberry_unit_tests PRIVATE CPIO_FIXTURE="${CPIO_FIXTURE}")
