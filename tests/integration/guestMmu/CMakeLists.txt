add_library(TestGuestMmu STATIC test_guestMmu.cpp)

target_link_libraries(TestGuestMmu
  GuestMmu
  PageTable
  Pmm
  Uart
)
