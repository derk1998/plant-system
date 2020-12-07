# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(plant-system)

#Export compile commands for You Complete Me
set( CMAKE_EXPORT_COMPILE_COMMANDS ON )

if( EXISTS "${CMAKE_CURRENT_BINARY_DIR}/compile_commands.json" )
    execute_process( COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_CURRENT_BINARY_DIR}/compile_commands.json
    ${CMAKE_CURRENT_SOURCE_DIR}/compile_commands.json)
endif()


