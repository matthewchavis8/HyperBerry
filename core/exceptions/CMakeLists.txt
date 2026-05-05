add_subdirectory(hvc)

add_library(Exceptions
  STATIC
  exceptions.cpp
)

target_link_libraries(Exceptions
  PRIVATE
  Hvc
  Uart
)
