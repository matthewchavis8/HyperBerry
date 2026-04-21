add_library(Vm
  STATIC
  vm.cpp
)

target_link_libraries(Vm
  PRIVATE
  GuestMmu
  Pmm
  Vcpu
  Uart
)
