add_library(Hvc
  STATIC
  hvc.cpp
)

target_link_libraries(Hvc
  PRIVATE
  Uart
)
