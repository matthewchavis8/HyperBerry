add_library(TestBuddy STATIC test_buddy.cpp)
target_link_libraries(TestBuddy Buddy Uart)
