cmake_minimum_required (VERSION 3.8)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD 11)
project (Tinyhttpd++ CXX C)

set(CMAKE_CXX_COMPILER gcc)
add_executable (C_Tinyhttpd "src/httpd.c")
add_executable (C_TinyhttpdClient "src/simpleclient.c")

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/bin/app/)

add_custom_command(TARGET C_Tinyhttpd POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_directory
                   ${CMAKE_SOURCE_DIR}/res/
                   ${EXECUTABLE_OUTPUT_PATH}/res/)