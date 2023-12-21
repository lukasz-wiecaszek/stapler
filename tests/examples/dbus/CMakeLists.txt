cmake_minimum_required(VERSION 3.5)

project(dbus-examples VERSION 1.0.0)

# sets various paths used in e.g. pc.in files as well as install target
include(GNUInstallDirs)

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

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING
         "Choose the type of build. Options are: {Release, Debug}." FORCE)
endif(NOT CMAKE_BUILD_TYPE)

message(STATUS "CMAKE_BUILD_TYPE: " ${CMAKE_BUILD_TYPE})

find_package(PkgConfig REQUIRED)
pkg_check_modules(DBUS REQUIRED dbus-1)

include_directories(${DBUS_INCLUDE_DIRS})
link_directories(${DBUS_LIBRARY_DIRS})

add_executable(ping-client ping-client.c)
target_link_libraries(ping-client ${DBUS_LIBRARIES})

add_executable(ping-server ping-server.c)
target_link_libraries(ping-server ${DBUS_LIBRARIES})

add_executable(calculator-client calculator-client.c)
target_link_libraries(calculator-client ${DBUS_LIBRARIES})

add_executable(calculator-server calculator-server.c)
target_link_libraries(calculator-server ${DBUS_LIBRARIES})

add_executable(timer-client timer-client.c)
target_link_libraries(timer-client ${DBUS_LIBRARIES})

add_executable(timer-server timer-server.c)
target_link_libraries(timer-server ${DBUS_LIBRARIES})