add_subdirectory(exceptions)

add_library(Core
  STATIC
  main.cpp
)

target_link_libraries(Core
  PRIVATE
  Uart
  Exceptions
)
