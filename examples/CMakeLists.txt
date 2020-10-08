cmake_minimum_required (VERSION 3.8)

add_executable (vkmerc "vkmerc.cpp" )

target_link_libraries(vkmerc base
	${Vulkan_LIBRARY}
	${shader_LIBRARIES}
	${glfw3_LIBRARY})