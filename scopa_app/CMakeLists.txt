cmake_minimum_required(VERSION 3.21)
project(scopa_app CXX)
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
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../game     ${CMAKE_BINARY_DIR}/game_build)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../tabletop ${CMAKE_BINARY_DIR}/tabletop_build)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../scopa    ${CMAKE_BINARY_DIR}/scopa_build)

# online_lib is a prerequisite of giocamo on native builds.
if(NOT EMSCRIPTEN)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../online ${CMAKE_BINARY_DIR}/online_build)
endif()
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../giocamo ${CMAKE_BINARY_DIR}/giocamo_build)

add_executable(scopa_app
    main.cpp
    ui.cpp
    agent_ui.cpp
)
target_include_directories(scopa_app PRIVATE
    .
    "${CMAKE_CURRENT_SOURCE_DIR}/.."  # so headers can use <game/x.h>, <scopa/x.h>.
)
target_link_libraries(scopa_app PRIVATE
    game
    scopa_lib
    tabletop_lib
    raylib
    giocamo
)
if(NOT EMSCRIPTEN)
    target_link_libraries(scopa_app PRIVATE online_lib)
endif()
