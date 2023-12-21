
# get file name with neither the directory nor the longest extension
get_filename_component(THRIFT_FILE_WE ${THRIFT_FILE} NAME_WE)

# directory for auto-generated thrift files
set(THRIFT_OUTPUT_DIR
    ${CMAKE_CURRENT_BINARY_DIR}/${THRIFT_FILE_WE}
)
message(STATUS "THRIFT_OUTPUT_DIR: " ${THRIFT_OUTPUT_DIR})

message(STATUS "Removing previous instance of auto-generated thrift files")
file(REMOVE_RECURSE ${THRIFT_OUTPUT_DIR})

message(STATUS "Generating thrift files for '${THRIFT_FILE}'")
file(MAKE_DIRECTORY ${THRIFT_OUTPUT_DIR})
execute_process(
    COMMAND
        ${THRIFT_HOST_COMPILER} -out ${THRIFT_OUTPUT_DIR} -r --gen cpp:no_skeleton ${CMAKE_CURRENT_SOURCE_DIR}/${THRIFT_FILE}
    RESULT_VARIABLE
        EXECUTE_PROCESS_STATUS
)
if(NOT EXECUTE_PROCESS_STATUS EQUAL 0)
    message(FATAL_ERROR "Generating thrift files for '${THRIFT_FILE}' failed")
endif()

file(GLOB THRIFT_PUBLIC_HEADERS "${THRIFT_OUTPUT_DIR}/*.h")
file(GLOB THRIFT_SOURCE_FILES "${THRIFT_OUTPUT_DIR}/*.cpp")

#------------------------------------------------------------------------------
#                                    TARGET
#------------------------------------------------------------------------------
set(TARGET_NAME thrift-${THRIFT_FILE_WE})
message(STATUS "TARGET_NAME: " ${TARGET_NAME})

add_library(${TARGET_NAME}
    ${THRIFT_SOURCE_FILES}
)

target_compile_options(${TARGET_NAME}
    PUBLIC
        -fPIC
)

target_include_directories(${TARGET_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${THRIFT_OUTPUT_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${TARGET_NAME}>
)

target_link_libraries(${TARGET_NAME}
    PUBLIC
        ${THRIFT_LIBRARIES}
)

set_target_properties(${TARGET_NAME}
  PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    PUBLIC_HEADER "${THRIFT_PUBLIC_HEADERS}"
)

#------------------------------------------------------------------------------
#                                 CONFIGURATION
#------------------------------------------------------------------------------
# create a config file for a project
configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}-config.cmake"
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${TARGET_NAME}
  NO_SET_AND_CHECK_MACRO
)

# generate the version file for the config file
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}-config-version.cmake"
  VERSION "${PROJECT_VERSION}"
  COMPATIBILITY AnyNewerVersion
)

#------------------------------------------------------------------------------
#                                 INSTALLATION
#------------------------------------------------------------------------------
install(TARGETS ${TARGET_NAME}
  EXPORT ${TARGET_NAME}Targets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${TARGET_NAME}
)

install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}-config.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}-config-version.cmake"
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${TARGET_NAME}
)

# Install the export set for use with the install-tree
install(EXPORT ${TARGET_NAME}Targets
  FILE ${TARGET_NAME}Targets.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${TARGET_NAME}
)

# Add all targets to the build-tree export set
export(EXPORT ${TARGET_NAME}Targets
  FILE "${CMAKE_CURRENT_BINARY_DIR}/cmake/${TARGET_NAME}-targets.cmake"
)
