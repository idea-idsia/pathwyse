#Minimum CMake version
cmake_minimum_required(VERSION 3.20)

#Project information
set(PROJECT_VERSION 1.1)
project(pathwyse VERSION ${PROJECT_VERSION} DESCRIPTION "PathWyse: an open source library for RCSPP")
if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

#Source code
add_subdirectory(src)

