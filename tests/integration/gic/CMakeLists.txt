add_library(TestGic
  STATIC
  test_gic.cpp
  test_vectors.S
  guest_payload.S
)

target_link_libraries(TestGic
  Gic
  Vcpu
  Uart
)
