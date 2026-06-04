cmake_minimum_required(VERSION 3.21)
project(dot CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(DOT_SOURCES
    cards.cpp
)

# game static lib (idempotent: skipped if already added by a parent project).
if(NOT TARGET game)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../game" game_build)
endif()

if(NOT TARGET dot_lib)
    add_library(dot_lib STATIC ${DOT_SOURCES})
    target_include_directories(dot_lib PUBLIC . "${CMAKE_CURRENT_SOURCE_DIR}/..")
    target_link_libraries(dot_lib PUBLIC game)
endif()
