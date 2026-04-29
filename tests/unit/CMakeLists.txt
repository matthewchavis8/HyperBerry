add_executable(hyperberry_unit_tests
  array/test_array.cpp
  pageTable/test_pageTable.cpp
  hostMmu/test_hostMmu.cpp
  guestMmu/test_guestMmu.cpp
  vcpu/test_vcpu.cpp
  exceptions/test_exceptions.cpp

  utility/test_utility.cpp
  memory/test_unique_ptr.cpp
  memory/test_shared_ptr.cpp

  strings/test_strings.cpp
  ${CMAKE_SOURCE_DIR}/lib/strings/strings.cpp

  dtb/test_dtb.cpp
  uart/test_uart_format.cpp
  ${CMAKE_SOURCE_DIR}/core/dtb/dtb.cpp

  bootpkg/test_bootpkg.cpp
  ${CMAKE_SOURCE_DIR}/core/bootpkg/bootpkg.cpp

  vm/test_vm.cpp
  ${CMAKE_SOURCE_DIR}/core/vm/vm.cpp
)

target_include_directories(hyperberry_unit_tests
  PRIVATE
  ${CMAKE_SOURCE_DIR}
)

target_compile_definitions(hyperberry_unit_tests
  PRIVATE
  PLATFORM_QEMU
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
)

target_compile_definitions(hyperberry_heap_tests
  PRIVATE
  PLATFORM_QEMU
  HEAP_TESTING_BUILD
)

target_link_libraries(hyperberry_heap_tests
  PRIVATE
  GTest::gtest_main
)

gtest_discover_tests(hyperberry_heap_tests)
