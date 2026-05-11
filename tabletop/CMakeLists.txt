cmake_minimum_required(VERSION 3.21)
project(tabletop CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(KT_SOURCES
    models.cpp
    config.cpp
    game_state.cpp
    input.cpp
    rendering.cpp
    ui.cpp
)

if(TARGET raylib AND NOT TARGET tabletop_lib)
    add_library(tabletop_lib STATIC ${KT_SOURCES})
    target_include_directories(tabletop_lib PUBLIC .)
    target_link_libraries(tabletop_lib PUBLIC raylib)
endif()
