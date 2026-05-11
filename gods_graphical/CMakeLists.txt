cmake_minimum_required(VERSION 3.21)
project(gods_graphical CXX)
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

# nlohmann_json for online_lib.
FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(nlohmann_json)

# Tell sibling libraries to skip their nanobind Python module targets when
# pulled in from here — we only need the pure C++ static libraries.
set(STANDALONE_CPP_ONLY ON CACHE BOOL "" FORCE)

# Pull in the sibling libraries. Each one exposes a *_lib static target when
# raylib (or nlohmann_json) is already defined by this parent project.
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../game           ${CMAKE_BINARY_DIR}/game_build)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../tabletop  ${CMAKE_BINARY_DIR}/kt_build)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../gods           ${CMAKE_BINARY_DIR}/gods_build)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../online         ${CMAKE_BINARY_DIR}/online_build)

add_executable(gods_graphical
    main.cpp
    ui.cpp
    agent_ui.cpp
    menu.cpp
    agent_remote.cpp
)
target_include_directories(gods_graphical PRIVATE
    .
    "${CMAKE_CURRENT_SOURCE_DIR}/.."  # so headers can use <game/x.h>
)
target_link_libraries(gods_graphical PRIVATE
    game
    gods_lib
    tabletop_lib
    online_lib
    raylib
    nlohmann_json::nlohmann_json
)
