add_library(Vcpu
  STATIC
  vcpu.cpp
)

target_link_libraries(Vcpu
  PRIVATE
  Strings
)
