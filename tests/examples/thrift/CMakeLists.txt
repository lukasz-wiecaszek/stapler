cmake_minimum_required(VERSION 3.5)

project(thrift-examples VERSION 1.0.0)

# sets various paths used in e.g. pc.in files as well as install target
include(GNUInstallDirs)

# Helpers functions for creating config files that can be included by other projects to find and use a package.
# Adds the configure_package_config_file() and write_basic_package_version_file() commands.
include(CMakePackageConfigHelpers)

message(STATUS "Processing CMakeLists.txt for: " ${PROJECT_NAME} " " ${PROJECT_VERSION})

# if you are building in-source, this is the same as CMAKE_SOURCE_DIR, otherwise
# this is the top level directory of your build tree
message(STATUS "CMAKE_BINARY_DIR:         " ${CMAKE_BINARY_DIR})

# if you are building in-source, this is the same as CMAKE_CURRENT_SOURCE_DIR, otherwise this
# is the directory where the compiled or generated files from the current CMakeLists.txt will go to
message(STATUS "CMAKE_CURRENT_BINARY_DIR: " ${CMAKE_CURRENT_BINARY_DIR})

# this is the directory, from which cmake was started, i.e. the top level source directory
message(STATUS "CMAKE_SOURCE_DIR:         " ${CMAKE_SOURCE_DIR})

# this is the directory where the currently processed CMakeLists.txt is located in
message(STATUS "CMAKE_CURRENT_SOURCE_DIR: " ${CMAKE_CURRENT_SOURCE_DIR})

set (CMAKE_CXX_STANDARD 20)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING
         "Choose the type of build. Options are: {Release, Debug}." FORCE)
endif(NOT CMAKE_BUILD_TYPE)

message(STATUS "CMAKE_BUILD_TYPE: " ${CMAKE_BUILD_TYPE})

option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)
option(USE_STAPLER_TRANSPORT "Use custom stapler based thrift transport" OFF)

if(USE_STAPLER_TRANSPORT)
    add_definitions(-DUSE_STAPLER_TRANSPORT)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(THRIFT REQUIRED thrift)

include_directories(${THRIFT_INCLUDE_DIRS})
link_directories(${THRIFT_LIBRARY_DIRS})

set(THRIFT_HOST_COMPILER thrift)
set(THRIFT_FILES
    ping.thrift
    calculator.thrift
    timer.thrift)

foreach(THRIFT_FILE IN LISTS THRIFT_FILES)
include(thrift.include.cmake)
endforeach()

add_executable(ping-client ping-client.cpp)
target_link_libraries(ping-client thrift-ping)

add_executable(ping-server ping-server.cpp)
target_link_libraries(ping-server thrift-ping)

add_executable(calculator-client calculator-client.cpp)
target_link_libraries(calculator-client thrift-calculator)

add_executable(calculator-server calculator-server.cpp)
target_link_libraries(calculator-server thrift-calculator)

add_executable(timer-client timer-client.cpp)
target_link_libraries(timer-client thrift-timer)

add_executable(timer-server timer-server.cpp)
target_link_libraries(timer-server thrift-timer)