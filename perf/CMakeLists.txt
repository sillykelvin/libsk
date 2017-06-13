file(GLOB_RECURSE SRC_LIST *.h *.c *.cpp)

set(EXECUTABLE_OUTPUT_PATH "${PROJECT_SOURCE_DIR}/lib")

include_directories("${PROJECT_SOURCE_DIR}/src")
include_directories("${PROJECT_SOURCE_DIR}/deps/pugixml/include")
include_directories("${PROJECT_SOURCE_DIR}/deps/spdlog/include")
include_directories("${PROJECT_SOURCE_DIR}/deps/curl/include")

link_directories("${PROJECT_SOURCE_DIR}/lib")

add_executable(test-perf ${SRC_LIST})
target_link_libraries(test-perf sk)
