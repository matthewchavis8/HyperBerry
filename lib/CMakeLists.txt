add_library(Lib${BSP_SUFFIX}
  STATIC
  cpio/cpio.cpp
  cxxrt/cxxrt.cpp
  log/log.cpp
  panic/panic.cpp
  strings/strings.cpp
)

target_include_directories(Lib${BSP_SUFFIX}
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/panic
  ${CMAKE_CURRENT_SOURCE_DIR}/strings
)
