add_library(TestMmu STATIC test_mmu.cpp)

target_link_libraries(TestMmu
  HostMmu
  PageTable
  Pmm
  Uart
)
