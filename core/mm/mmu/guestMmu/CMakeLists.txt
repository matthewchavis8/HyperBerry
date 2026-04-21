add_library(GuestMmu
  STATIC
  guestMmu.cpp
)

target_link_libraries(GuestMmu
  PRIVATE
  PageTable
  Pmm
  Uart
)
