cmake_minimum_required(VERSION 3.21)
project(tabletop_cpp CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(KT_SOURCES
    code/models.cpp
    code/config.cpp
    code/game_state.cpp
    code/input.cpp
    code/rendering.cpp
    code/ui.cpp
)

if(TARGET raylib AND NOT TARGET tabletop_cpp_lib)
    add_library(tabletop_cpp_lib STATIC ${KT_SOURCES})
    target_include_directories(tabletop_cpp_lib PUBLIC code)
    target_link_libraries(tabletop_cpp_lib PUBLIC raylib)
endif()
