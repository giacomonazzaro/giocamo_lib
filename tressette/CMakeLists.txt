cmake_minimum_required(VERSION 3.21)
project(tressette_cpp CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# game static lib (idempotent: skipped if already added by a parent project).
if(NOT TARGET game)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../game" game_build)
endif()

# Header-only interface library (C++ files merged into headers).
if(NOT TARGET tressette_lib)
    add_library(tressette_lib INTERFACE)
    target_include_directories(tressette_lib INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/.."
    )
    target_link_libraries(tressette_lib INTERFACE game)
endif()