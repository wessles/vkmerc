file(GLOB_RECURSE BASE_SRC "*.cpp" "*.hpp" "*.h")
message(${BASE_SRC})
add_library(base STATIC ${BASE_SRC})
set_target_properties(base PROPERTIES LINKER_LANGUAGE CXX)