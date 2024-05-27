cmake_minimum_required(VERSION 3.12)

file(GLOB SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.h)

file(MAKE_DIRECTORY ${CMAKE_HEADER_OUTPUT_DIRECTORY})

set(OUTPUT "${CMAKE_HEADER_OUTPUT_DIRECTORY}/json20.h")

add_custom_command(
    OUTPUT "${CMAKE_HEADER_OUTPUT_DIRECTORY}/json20.h"
    COMMAND to_single_header ARGS json_single.h ${CMAKE_HEADER_OUTPUT_DIRECTORY}/json20.h
    DEPENDS ${SOURCES}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Generating single header"
    VERBATIM
)

add_custom_target(libjson20_single_header ALL DEPENDS ${OUTPUT})
add_dependencies(libjson20_single_header to_single_header)



