set(TCP_ECHO_SERVER_SRC "tcp_echo_server.cpp")
set(TCP_ECHO_CLIENT_SRC "tcp_echo_client.cpp")
set(REST_CLIENT_SRC "rest_client/main.cpp")
set(CONSUL_CLIENT_SRC "consul_client/main.cpp")

set(EXECUTABLE_OUTPUT_PATH "${PROJECT_SOURCE_DIR}/lib")

include_directories("${PROJECT_SOURCE_DIR}/src")
include_directories("${PROJECT_SOURCE_DIR}/deps/pugixml/include")
include_directories("${PROJECT_SOURCE_DIR}/deps/spdlog/include")
include_directories("${PROJECT_SOURCE_DIR}/deps/curl/include")
include_directories("${PROJECT_SOURCE_DIR}/deps/json/include")

link_directories("${PROJECT_SOURCE_DIR}/lib")
link_directories("${PROJECT_SOURCE_DIR}/deps/curl/lib")

add_executable(example-tcp-echo-server ${TCP_ECHO_SERVER_SRC})
add_executable(example-tcp-echo-client ${TCP_ECHO_CLIENT_SRC})
add_executable(example-rest-client     ${REST_CLIENT_SRC})
add_executable(example-consul-client   ${CONSUL_CLIENT_SRC})
target_link_libraries(example-tcp-echo-server sk)
target_link_libraries(example-tcp-echo-client sk)
target_link_libraries(example-rest-client sk curl ssl crypto z)
target_link_libraries(example-consul-client sk curl ssl crypto z)
