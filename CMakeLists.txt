cmake_minimum_required (VERSION 3.8)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD 11)
project (Tinyhttpd++ CXX C)

SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g2 -ggdb") # 定义Debug编译参数
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall") # 定义Release编译参数

set(CMAKE_CXX_COMPILER g++)
add_executable (C_Tinyhttpd "src/edited_httpd.c")

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/bin/app/)

add_custom_command(TARGET C_Tinyhttpd POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_directory
                   ${CMAKE_SOURCE_DIR}/res/
                   ${EXECUTABLE_OUTPUT_PATH}/res/)