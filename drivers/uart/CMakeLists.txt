add_library(Uart
  STATIC
  uart.cpp
)

target_include_directories(Uart
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
)
