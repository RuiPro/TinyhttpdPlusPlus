cmake_minimum_required (VERSION 3.8)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD 11)
project ("Tinyhttpd++" CXX C)

set(CMAKE_CXX_COMPILER gcc)
add_executable (C_Tinyhttpd "src/httpd.c")
add_executable (C_TinyhttpdClient "src/simpleclient.c")

set(LIBRARY_OUTPUT_PATH bin/lib/)
set(EXECUTABLE_OUTPUT_PATH bin/app/)