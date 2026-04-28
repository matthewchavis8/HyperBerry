add_library(Heap
  STATIC
  heap.cpp
)

# Match the freestanding kernel build flags so noexcept paths in
# operator delete do not pull in __cxa_begin_catch / std::terminate.
target_compile_options(Heap
  PRIVATE
  -fno-exceptions
  -fno-rtti
)
