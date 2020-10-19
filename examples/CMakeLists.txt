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
demo("HelmetDemo")
demo("EmptyDemo")

file(GLOB_RECURSE RESOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/res/*.)
add_custom_target(CHECK_FILE_LISTS ALL ${RESOURCE_FILES})

# copy resources folder to binary folder
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/res DESTINATION ${CMAKE_CURRENT_BINARY_DIR})