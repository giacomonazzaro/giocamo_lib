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
# Tell raylib to build for Web + WebGL2 when cross-compiling with Emscripten.
if(EMSCRIPTEN)
    set(PLATFORM "Web" CACHE STRING "" FORCE)
    set(GRAPHICS "GRAPHICS_API_OPENGL_ES3" CACHE STRING "" FORCE)
endif()
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

if(NOT EMSCRIPTEN)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../online ${CMAKE_BINARY_DIR}/online_build)
endif()

# agent_remote.cpp uses the online module which is unavailable in Emscripten
# builds; stubs in agent_remote.h cover that path instead.
if(EMSCRIPTEN)
    set(GODS_GRAPHICAL_SOURCES main.cpp ui.cpp agent_ui.cpp menu.cpp)
else()
    set(GODS_GRAPHICAL_SOURCES main.cpp ui.cpp agent_ui.cpp menu.cpp agent_remote.cpp)
endif()

add_executable(gods_graphical ${GODS_GRAPHICAL_SOURCES})
target_include_directories(gods_graphical PRIVATE
    .
    "${CMAKE_CURRENT_SOURCE_DIR}/.."  # so headers can use <game/x.h>
)
target_link_libraries(gods_graphical PRIVATE
    game
    gods_lib
    tabletop_lib
    raylib
    nlohmann_json::nlohmann_json
)
if(NOT EMSCRIPTEN)
    target_link_libraries(gods_graphical PRIVATE online_lib)
endif()

if(EMSCRIPTEN)
    # Flags from the raylib Working-for-Web guide.
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -s USE_GLFW=3 -s ASSERTIONS=1 -s WASM=1 -s ASYNCIFY -s GL_ENABLE_GET_PROC_ADDRESS=1")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s USE_GLFW=3 -s ASSERTIONS=1 -s WASM=1 -s ASYNCIFY -s GL_ENABLE_GET_PROC_ADDRESS=1")
    set(CMAKE_EXECUTABLE_SUFFIX ".html")

    # Bundle all game assets into the .data file for the browser.
    set(ASSETS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} \
        -s TOTAL_MEMORY=268435456 \
        -s MAX_WEBGL_VERSION=2 \
        -s MIN_WEBGL_VERSION=2 \
        --shell-file ${raylib_SOURCE_DIR}/src/minshell.html \
        --preload-file ${ASSETS_ROOT}/gods/cards.json@/gods/cards.json \
        --preload-file ${ASSETS_ROOT}/gods/card-images@/gods/card-images \
        --preload-file ${ASSETS_ROOT}/fonts@/fonts \
        --preload-file ${ASSETS_ROOT}/tabletop/background.frag@/tabletop/background.frag \
        --preload-file ${ASSETS_ROOT}/data/debug_gods_state.json@/data/debug_gods_state.json \
        --preload-file ${ASSETS_ROOT}/data/debug_table_state.json@/data/debug_table_state.json")
endif()
