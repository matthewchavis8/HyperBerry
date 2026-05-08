add_library(TestTimer
  STATIC
  test_timer.cpp
  test_vectors.S
)

target_link_libraries(TestTimer
  Timer
  Gic
  Uart
)
