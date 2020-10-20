cmake_minimum_required (VERSION 3.8)


# function to add a demo of {name}.cpp to targets


function(demo dname)
	add_executable (${dname} ${dname}.cpp)
	target_link_libraries(${dname} base
		${Vulkan_LIBRARY}
		${shader_LIBRARIES}
		${glfw3_LIBRARY})
endfunction()

# list all demos here
demo("CascadedShadowmapDemo")
demo("KhrRaytraceDemo")
demo("MultisampleBlitDemo")
demo("HelmetDemo")
demo("EmptyDemo")

file(CREATE_LINK "${CMAKE_SOURCE_DIR}/examples/res/" "${CMAKE_CURRENT_BINARY_DIR}/res/" SYMBOLIC)