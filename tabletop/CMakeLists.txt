cmake_minimum_required(VERSION 3.21)
project(tabletop CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(TABLETOP_SOURCES
    config.cpp
    tabletop.cpp
    input_recorder.cpp
    rendering.cpp
    ui.cpp
)

if(TARGET raylib AND NOT TARGET tabletop_lib)
    add_library(tabletop_lib STATIC ${TABLETOP_SOURCES})
    # Add parent dir so we can include <struct/json.h> for input recording.
    target_include_directories(tabletop_lib PUBLIC . "${CMAKE_CURRENT_SOURCE_DIR}/..")
    target_link_libraries(tabletop_lib PUBLIC raylib)
endif()
