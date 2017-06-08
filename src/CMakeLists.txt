set(PY_SCRIPT "${PROJECT_SOURCE_DIR}/tools/config/gen_source.py")
set(LOG_CONFIG_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/log/log_config.h")
set(LOG_CONFIG_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/log/log_config.cpp")

add_custom_command(
  OUTPUT ${LOG_CONFIG_SOURCE}
  COMMAND python ${PY_SCRIPT} ${LOG_CONFIG_HEADER} ${LOG_CONFIG_SOURCE}
  DEPENDS ${LOG_CONFIG_HEADER}
  VERBATIM)

file(GLOB_RECURSE SRC_LIST *.h *.c *.cpp)
list(APPEND SRC_LIST ${LOG_CONFIG_SOURCE})

set(LIBRARY_OUTPUT_PATH "${PROJECT_SOURCE_DIR}/lib")

include_directories("${PROJECT_SOURCE_DIR}/deps/pugixml/include")
include_directories("${PROJECT_SOURCE_DIR}/deps/spdlog/include")

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
add_library(sk ${SRC_LIST})
