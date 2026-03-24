cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) 
set(CMAKE_OBJCOPY llvm-objcopy)

project(HyperBerry VERSION 0.0.1 LANGUAGES CXX ASM C)

# Build option for targets
set(BOARD "rpi5" CACHE STRING "Target Board")

if (BOARD STREQUAL "rpi5")
  message(STATUS "Building for target: Raspberry Pi 5")
elseif (BOARD STREQUAL "qemu")
  message(STATUS "Building for target: Virtualized Raspberry Pi 5 in QEMU")
else ()
  message(STATUS "[ERROR] unknown board detected={BOARD}")
endif()

# Does this compile
add_executable(
  TEST
  test.cpp
)

# Pull in Google Test
# COMMENT FOR NOW need to seperate debug and release later
# include(CTest)
# if (BUILD_TESTING)
#   # For MSVC compilers avoid conflict
#   set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
#
#   include(FetchContent)
#   FetchContent_Declare(
#     googletest
#     URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
#   )
#   FetchContent_MakeAvailable(googletest)
#
#   add_subdirectory(tests)
# endif()
