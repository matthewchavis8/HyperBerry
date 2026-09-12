add_library(Mm${BSP_SUFFIX}
  STATIC
  pmm/pmm.cpp
  heap/heap.cpp
  pageTable/pageTable.cpp
  mmu/hostMmu/hostMmu.cpp
  mmu/guestMmu/guestMmu.cpp
)

target_include_directories(Mm${BSP_SUFFIX}
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/pmm
  ${CMAKE_CURRENT_SOURCE_DIR}/heap
  ${CMAKE_CURRENT_SOURCE_DIR}/pageTable
  ${CMAKE_CURRENT_SOURCE_DIR}/mmu
  ${CMAKE_CURRENT_SOURCE_DIR}/mmu/hostMmu
  ${CMAKE_CURRENT_SOURCE_DIR}/mmu/guestMmu
)

target_link_libraries(Mm${BSP_SUFFIX}
  PUBLIC
  Drivers${BSP_SUFFIX}
  Lib${BSP_SUFFIX}
)
