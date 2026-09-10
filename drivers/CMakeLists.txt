add_library(Drivers${BSP_SUFFIX}
  STATIC
  uart/uart.cpp
  gic/gic.cpp
  timer/timer.cpp
)

target_include_directories(Drivers${BSP_SUFFIX}
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/uart
  ${CMAKE_CURRENT_SOURCE_DIR}/gic
  ${CMAKE_CURRENT_SOURCE_DIR}/timer
)

target_link_libraries(Drivers${BSP_SUFFIX}
  PUBLIC
  Lib${BSP_SUFFIX}
)
