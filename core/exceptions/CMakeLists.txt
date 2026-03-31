add_library(Exceptions
  STATIC
  exceptions.cpp
)

target_link_libraries(Exceptions
  PRIVATE
  Uart
)
