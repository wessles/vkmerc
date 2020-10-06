cmake_minimum_required (VERSION 3.8)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project (vkmerc C CXX)

find_package(Vulkan REQUIRED FATAL_ERROR)
include_directories(${Vulkan_INCLUDE_DIRS})

set(glfw3_LIBRARY ${CMAKE_CURRENT_SOURCE_DIR}/lib/glfw3.lib)

include_directories(external)
include_directories(base)

add_subdirectory(base)
add_subdirectory(examples)