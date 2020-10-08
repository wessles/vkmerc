cmake_minimum_required (VERSION 3.8)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project (vkmerc C CXX)

find_package(Vulkan REQUIRED FATAL_ERROR)
include_directories(${Vulkan_INCLUDE_DIRS})

set(glfw3_LIBRARY ${CMAKE_CURRENT_SOURCE_DIR}/lib/glfw3.lib)

set(Vulkan_LIBRARY_DIR "${Vulkan_INCLUDE_DIRS}/../Lib/")
set(shader_LIBRARIES
	"${Vulkan_LIBRARY_DIR}/shaderc_combined.lib"
	"${Vulkan_LIBRARY_DIR}/SPIRV.lib"
	"${Vulkan_LIBRARY_DIR}/spirv-cross-glsl.lib"
	"${Vulkan_LIBRARY_DIR}/spirv-cross-cpp.lib"
	"${Vulkan_LIBRARY_DIR}/spirv-cross-core.lib"
	"${Vulkan_LIBRARY_DIR}/spirv-cross-reflect.lib")

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MD")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MD")

include_directories(external)
include_directories(base)

add_subdirectory(base)
add_subdirectory(examples)