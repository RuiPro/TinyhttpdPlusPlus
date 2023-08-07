cmake_minimum_required (VERSION 3.8)
set(CMAKE_CXX_STANDARD 11)
project ("Tinyhttpd++" CXX C)
set(CMAKE_CXX_COMPILER g++)

add_executable (Tinyhttpd++ "httpd.cpp")

set(LIBRARY_OUTPUT_PATH /root/app/${PROJECT_NAME})
set(EXECUTABLE_OUTPUT_PATH /root/app/${PROJECT_NAME})