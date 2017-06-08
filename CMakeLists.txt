cmake_minimum_required(VERSION 2.8)
project(libsk)

set(CMAKE_VERBOSE_MAKEFILE on)

add_definitions(-g -Wall -Werror -Wextra -rdynamic -fPIC -std=c++0x)
add_definitions(-pipe -D_NEW_LIC -D_GNU_SOURCE -D_REENTRANT)
add_definitions(-fno-strict-aliasing -Winvalid-pch)

add_subdirectory(src)
add_subdirectory(test)
add_subdirectory(perf)
