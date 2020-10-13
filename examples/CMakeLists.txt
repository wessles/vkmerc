cmake_minimum_required (VERSION 3.8)

function(demo dname)
add_executable (${dname} ${dname}.cpp)
target_link_libraries(${dname} base
	${Vulkan_LIBRARY}
	${shader_LIBRARIES}
	${glfw3_LIBRARY})
endfunction()

demo("CascadedShadowmapDemo")
demo("HelmetDemo")
demo("EmptyDemo")