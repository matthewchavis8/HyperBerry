add_executable(hyperberry_unit_tests
  array/test_array.cpp
  mmu/test_mmu.cpp

  stubs/uart_stub.cpp
  stubs/linker_stubs.cpp

  strings/test_strings.cpp
  ${CMAKE_SOURCE_DIR}/lib/strings/strings.cpp

  dtb/test_dtb.cpp
  ${CMAKE_SOURCE_DIR}/core/dtb/dtb.cpp
)

target_include_directories(hyperberry_unit_tests
  PRIVATE
  ${CMAKE_SOURCE_DIR}
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
