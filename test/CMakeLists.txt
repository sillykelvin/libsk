file(GLOB_RECURSE SRC_LIST *.h *.c *.cpp)

include_directories("${PROJECT_SOURCE_DIR}/src")
include_directories("${PROJECT_SOURCE_DIR}/deps/pugixml/include")
include_directories("${PROJECT_SOURCE_DIR}/deps/spdlog/include")

link_directories("${PROJECT_SOURCE_DIR}/lib")

add_executable(test-sk ${SRC_LIST})
target_link_libraries(test-sk sk gtest)
