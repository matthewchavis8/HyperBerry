add_library(PageTable
  STATIC
  pageTable.cpp
)

target_link_libraries(PageTable
  PRIVATE
  Pmm
  Uart
  Strings
)
