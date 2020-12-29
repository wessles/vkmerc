file(GLOB_RECURSE BASE_SRC "*.cpp" "*.hpp" "*.h")
add_library(base STATIC ${BASE_SRC})
set_target_properties(base PROPERTIES LINKER_LANGUAGE CXX)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(base ${CMAKE_THREAD_LIBS_INIT})