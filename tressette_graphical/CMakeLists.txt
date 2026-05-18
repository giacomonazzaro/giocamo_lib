cmake_minimum_required(VERSION 3.21)
project(tressette_graphical CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# raylib is vendored as a sibling clone at the project root.
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../raylib ${CMAKE_BINARY_DIR}/raylib_build)

# Tell sibling libraries to skip their nanobind targets.
set(STANDALONE_CPP_ONLY ON CACHE BOOL "" FORCE)

# Pull in the sibling libraries. Each one exposes a *_lib static target when
# raylib is already defined by this parent project.
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../game      ${CMAKE_BINARY_DIR}/game_build)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../tabletop  ${CMAKE_BINARY_DIR}/tabletop_build)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../tressette ${CMAKE_BINARY_DIR}/tressette_build)

add_executable(tressette_graphical
    main.cpp
    ui.cpp
    agent_ui.cpp
)
target_include_directories(tressette_graphical PRIVATE
    .
    "${CMAKE_CURRENT_SOURCE_DIR}/.."  # so headers can use <game/x.h>, <tressette/x.h>.
)
target_link_libraries(tressette_graphical PRIVATE
    game
    tressette_lib
    tabletop_lib
    raylib
)
