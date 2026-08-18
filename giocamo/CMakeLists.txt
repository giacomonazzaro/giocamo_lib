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

# game gives the Agent base class the online agents derive from. Idempotent:
# skipped when a parent project has already added it.
if(NOT TARGET game)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../game ${CMAKE_BINARY_DIR}/game_build)
endif()

set(GIOCAMO_SOURCES
    menu.cpp
    play.cpp
    online.cpp
)

if(EMSCRIPTEN)
    # In the browser, emscripten_fetch wraps the browser's own fetch().
    list(APPEND GIOCAMO_SOURCES http_fetch.cpp)
else()
    find_package(CURL REQUIRED)
    list(APPEND GIOCAMO_SOURCES http_curl.cpp)
endif()

add_library(giocamo STATIC ${GIOCAMO_SOURCES})

# PUBLIC so consumers (e.g. gods_app) can include "giocamo/menu.h" and
# also resolve sibling headers like <online/protocol.h>, <tabletop/config.h>.
target_include_directories(giocamo PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# menu.h and play.h pull in tabletop/* and game/* headers, so propagate those
# libraries publicly. tabletop_lib is a sibling library — the parent project
# is expected to have added it via add_subdirectory before adding giocamo.
target_link_libraries(giocamo PUBLIC
    raylib
    tabletop_lib
    game
    nlohmann_json::nlohmann_json
)
if(NOT EMSCRIPTEN)
    target_link_libraries(giocamo PUBLIC CURL::libcurl)
endif()

target_compile_options(giocamo PRIVATE -Wall -Wextra -Wpedantic)

# Check that no message is lost, reordered or doubled. Native only: it is
# run from a terminal. See the comment at the top of test_online.cpp.
if(NOT EMSCRIPTEN AND NOT TARGET giocamo_test_online)
    add_executable(giocamo_test_online test_online.cpp)
    target_link_libraries(giocamo_test_online PRIVATE giocamo)
endif()
