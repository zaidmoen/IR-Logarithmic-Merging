cmake_minimum_required(VERSION 3.14)
project(ir_external_indexing CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -pedantic)
endif()

add_library(irindex
  src/index_io.cpp
  src/memory_index.cpp
  src/dynamic_base.cpp
  src/merge.cpp
  src/aux_main.cpp
  src/logarithmic.cpp) # YOUR TASK
target_include_directories(irindex PUBLIC include)

foreach(tool build_dynamic lookup verify gen_corpus)
  add_executable(${tool} tools/${tool}.cpp)
  target_link_libraries(${tool} irindex)
endforeach()

add_executable(test_main tests/test_main.cpp)
target_link_libraries(test_main irindex)
