# Board-agnostic suites. A board that needs a different implementation drops a
# file of the same basename in tests/bsp/<board>/ and it wins.
set(SHARED_TESTS
  uart/test_uart.cpp
  vmm/test_vmm.cpp
  vmm/test_vectors.S
  pmm/test_pmm.cpp
  mmu/test_mmu.cpp
  guestMmu/test_guestMmu.cpp
  guest/test_guest.cpp
  vcpu/test_vcpu.cpp
  vcpu/test_vectors.S
  gic/test_gic.cpp
  gic/test_vectors.S
  timer/test_timer.cpp
  timer/test_vectors.S
)

file(GLOB BSP_TESTS CONFIGURE_DEPENDS
  ${CMAKE_SOURCE_DIR}/tests/bsp/${BSP_BOARD}/*.cpp
  ${CMAKE_SOURCE_DIR}/tests/bsp/${BSP_BOARD}/*.S
)

set(BSP_TEST_NAMES "")
foreach(src IN LISTS BSP_TESTS)
  get_filename_component(base ${src} NAME)
  list(APPEND BSP_TEST_NAMES ${base})
  message(STATUS "  ${BSP_BOARD}: board-specific test ${base}")
endforeach()

set(TEST_SOURCES "")
foreach(src IN LISTS SHARED_TESTS)
  get_filename_component(base ${src} NAME)
  if(base IN_LIST BSP_TEST_NAMES)
    message(STATUS "  ${BSP_BOARD}: ${base} overridden by tests/bsp/${BSP_BOARD}/")
  else()
    list(APPEND TEST_SOURCES ${src})
  endif()
endforeach()

add_library(Tests${BSP_SUFFIX}
  STATIC
  ${TEST_SOURCES}
  ${BSP_TESTS}
)

target_include_directories(Tests${BSP_SUFFIX}
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(Tests${BSP_SUFFIX}
  PRIVATE
  Virt${BSP_SUFFIX}
  Mm${BSP_SUFFIX}
  Drivers${BSP_SUFFIX}
  Lib${BSP_SUFFIX}
)
