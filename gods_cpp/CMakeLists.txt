cmake_minimum_required(VERSION 3.21)
project(gods_cpp CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(GODS_SOURCES
    code/models.cpp
    code/gameplay.cpp
    code/cards.cpp
    code/setup.cpp
    code/ai.cpp
)

# game_cpp static lib (idempotent: skipped if already added by a parent project).
if(NOT TARGET game_cpp)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../game_cpp" game_cpp_build)
endif()

# ----- Standalone C++ static library (no Python) -----
if(NOT TARGET gods_cpp_lib)
    add_library(gods_cpp_lib STATIC ${GODS_SOURCES})
    target_include_directories(gods_cpp_lib PUBLIC code "${CMAKE_CURRENT_SOURCE_DIR}/..")
    target_link_libraries(gods_cpp_lib PUBLIC game_cpp)
endif()

