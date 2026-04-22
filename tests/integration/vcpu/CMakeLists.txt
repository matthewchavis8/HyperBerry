add_library(TestVcpu
  STATIC
  test_vcpu.cpp
  test_vectors.S
)

target_link_libraries(TestVcpu
  Vcpu
  Uart
)
