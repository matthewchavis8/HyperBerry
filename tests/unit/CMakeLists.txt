add_executable(hyperberry_unit_tests
  array/test_array.cpp
)

target_include_directories(hyperberry_unit_tests
  PRIVATE
  ${CMAKE_SOURCE_DIR}
)

target_link_libraries(hyperberry_unit_tests
  PRIVATE
  GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(hyperberry_unit_tests)
