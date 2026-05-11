cmake_minimum_required(VERSION 3.21)
project(gods CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(GODS_SOURCES
    models.cpp
    gameplay.cpp
    cards.cpp
    setup.cpp
    ai.cpp
)

# game static lib (idempotent: skipped if already added by a parent project).
if(NOT TARGET game)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../game" game_build)
endif()

# ----- Standalone C++ static library (no Python) -----
if(NOT TARGET gods_lib)
    add_library(gods_lib STATIC ${GODS_SOURCES})
    target_include_directories(gods_lib PUBLIC . "${CMAKE_CURRENT_SOURCE_DIR}/..")
    target_link_libraries(gods_lib PUBLIC game)
endif()

