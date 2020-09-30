file(GLOB BASE_SRC "*.cpp" "*.hpp" "*.h")

add_library(base STATIC ${BASE_SRC})
set_target_properties(base PROPERTIES LINKER_LANGUAGE CXX)