cmake_minimum_required(VERSION 3.21)
project(tressette_graphical CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch raylib so tabletop_lib can link against it.
include(FetchContent)
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(raylib)

# Tell sibling libraries to skip their nanobind targets.
set(STANDALONE_CPP_ONLY ON CACHE BOOL "" FORCE)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../game     ${CMAKE_BINARY_DIR}/game_build)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../tabletop ${CMAKE_BINARY_DIR}/kt_build)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/..             ${CMAKE_BINARY_DIR}/tressette_build)

add_executable(tressette_graphical main.cpp)
target_include_directories(tressette_graphical PRIVATE
    .
    "${CMAKE_CURRENT_SOURCE_DIR}/../.."  # so headers can use <game/x.h>, <tressette/x.h>, etc.
)
target_link_libraries(tressette_graphical PRIVATE
    game
    tressette_lib
    tabletop_lib
    raylib
)
