add_library(TestPmm STATIC test_pmm.cpp)
target_link_libraries(TestPmm Pmm Uart)
