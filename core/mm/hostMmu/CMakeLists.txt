add_library(HostMmu
  STATIC
  hostMmu.cpp
)

target_link_libraries(HostMmu
  PRIVATE
  PageTable
  Pmm
  Uart
  Strings
)
