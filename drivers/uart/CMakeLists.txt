add_library(Uart
  STATIC
  uart.cpp
)

target_include_directories(Uart
  PUBLIC 
  ${CMAKE_SOURCE_DIR}
)
