add_library(Gic
  STATIC
  gic.cpp
)

target_include_directories(Gic
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
)
