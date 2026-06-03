cmake_minimum_required(VERSION 3.21)
project(connect_four CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CONNECT_FOUR_SOURCES
    gameplay.cpp
    ai.cpp
)

# game static lib (idempotent: skipped if already added by a parent project).
if(NOT TARGET game)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../game" game_build)
endif()

if(NOT TARGET connect_four_lib)
    add_library(connect_four_lib STATIC ${CONNECT_FOUR_SOURCES})
    target_include_directories(connect_four_lib PUBLIC . "${CMAKE_CURRENT_SOURCE_DIR}/..")
    target_link_libraries(connect_four_lib PUBLIC game)
endif()
