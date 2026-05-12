cmake_minimum_required(VERSION 3.21)
project(tressette_cpp CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(TRESSETTE_SOURCES
    models.cpp
    gameplay.cpp
    ai.cpp
    setup.cpp
)

# game static lib (idempotent: skipped if already added by a parent project).
if(NOT TARGET game)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../game" game_build)
endif()

# Standalone C++ static library (no Python / nanobind).
if(NOT TARGET tressette_lib)
    add_library(tressette_lib STATIC ${TRESSETTE_SOURCES})
    target_include_directories(tressette_lib PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/.."
    )
    target_link_libraries(tressette_lib PUBLIC game)
endif()