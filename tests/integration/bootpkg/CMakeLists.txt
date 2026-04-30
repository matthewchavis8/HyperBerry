add_library(TestBootPkg STATIC test_bootpkg.cpp)

target_link_libraries(TestBootPkg
  BootPkg
  HostMmu
  Pmm
  Uart
)
