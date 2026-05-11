cmake_minimum_required(VERSION 3.21)
project(gods_graphical_cpp CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch raylib so kitchen_table_cpp_lib can link against it.
include(FetchContent)
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(raylib)

# nlohmann_json for online_cpp_lib.
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
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../game_cpp           ${CMAKE_BINARY_DIR}/game_cpp_build)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../kitchen_table_cpp  ${CMAKE_BINARY_DIR}/kt_build)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../gods_cpp           ${CMAKE_BINARY_DIR}/gods_build)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../online_cpp         ${CMAKE_BINARY_DIR}/online_build)

add_executable(gods_graphical_cpp
    code/main.cpp
    code/ui.cpp
    code/agent_ui.cpp
    code/menu.cpp
    code/agent_remote.cpp
)
target_include_directories(gods_graphical_cpp PRIVATE
    code
    "${CMAKE_CURRENT_SOURCE_DIR}/.."  # so headers can use <game_cpp/x.h>
)
target_link_libraries(gods_graphical_cpp PRIVATE
    game_cpp
    gods_cpp_lib
    kitchen_table_cpp_lib
    online_cpp_lib
    raylib
    nlohmann_json::nlohmann_json
)
