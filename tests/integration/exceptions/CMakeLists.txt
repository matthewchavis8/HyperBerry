add_library(TestExceptions
  STATIC
  test_exceptions.cpp
  test_vectors.S
)

target_link_libraries(TestExceptions
  Exceptions
  Uart
)
