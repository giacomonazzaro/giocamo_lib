cmake_minimum_required(VERSION 3.15)
project(giocamo CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Third-party dependencies are vendored as sibling clones at the project root
# (../raylib, ../nlohmann_json). Guards keep us from re-adding a target that a
# parent project has already created.

if(NOT TARGET raylib)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)
    # Tell raylib to build for Web + WebGL2 when cross-compiling with Emscripten.
    if(EMSCRIPTEN)
        set(PLATFORM "Web" CACHE STRING "" FORCE)
        set(GRAPHICS "GRAPHICS_API_OPENGL_ES3" CACHE STRING "" FORCE)
    endif()
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../raylib ${CMAKE_BINARY_DIR}/raylib_build)
endif()

if(NOT TARGET nlohmann_json::nlohmann_json)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../nlohmann_json ${CMAKE_BINARY_DIR}/nlohmann_json_build)
endif()

add_library(giocamo STATIC
    menu.cpp
)

# PUBLIC so consumers (e.g. gods_graphical) can include "giocamo/menu.h" and
# also resolve sibling headers like <online/protocol.h>, <tabletop/config.h>.
target_include_directories(giocamo PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# menu.h pulls in online/* and tabletop/* headers, so propagate those libs
# publicly. online_lib is unavailable under Emscripten (uses CURL); menu.cpp's
# web branch uses the online_stub.h shim instead. tabletop_lib and online_lib
# are sibling libraries — the parent project is expected to have added them
# via add_subdirectory before adding giocamo.
target_link_libraries(giocamo PUBLIC
    raylib
    tabletop_lib
    nlohmann_json::nlohmann_json
)
if(NOT EMSCRIPTEN)
    target_link_libraries(giocamo PUBLIC online_lib)
endif()

target_compile_options(giocamo PRIVATE -Wall -Wextra -Wpedantic)
