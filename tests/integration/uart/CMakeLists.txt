add_library(TestUart
  STATIC
  test_uart.cpp
)

target_link_libraries(TestUart
  Uart
)
