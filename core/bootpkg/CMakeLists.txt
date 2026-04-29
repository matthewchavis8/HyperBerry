add_library(BootPkg
  STATIC
  bootpkg.cpp
)

target_link_libraries(BootPkg
  PRIVATE
  HostMmu
  Pmm
  Strings
)
