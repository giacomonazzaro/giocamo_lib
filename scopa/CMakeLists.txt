cmake_minimum_required(VERSION 3.21)
project(scopa CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SCOPA_SOURCES
    gameplay.cpp
    ai.cpp
)

# game static lib (idempotent: skipped if already added by a parent project).
if(NOT TARGET game)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../game" game_build)
endif()

if(NOT TARGET scopa_lib)
    add_library(scopa_lib STATIC ${SCOPA_SOURCES})
    target_include_directories(scopa_lib PUBLIC . "${CMAKE_CURRENT_SOURCE_DIR}/..")
    target_link_libraries(scopa_lib PUBLIC game)
endif()
