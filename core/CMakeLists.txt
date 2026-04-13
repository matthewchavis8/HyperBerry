add_subdirectory(exceptions)
add_subdirectory(mm)
add_subdirectory(dtb)

add_library(Core
  STATIC
  main.cpp
)

target_link_libraries(Core
  PRIVATE
  Uart
  Exceptions
  Pmm
  Dtb
)
