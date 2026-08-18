cmake_minimum_required(VERSION 3.15)
project(online CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Online play over a Firebase Realtime Database. This library stands on its
# own: it uses game (for the Agent base class) and nlohmann_json, and nothing
# else from giocamo. See firebase_setup.md.

# Dependencies are vendored as clones at the project root. Guards keep us from
# re-adding a target a parent project has already created.
if(NOT TARGET nlohmann_json::nlohmann_json)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../nlohmann_json ${CMAKE_BINARY_DIR}/nlohmann_json_build)
endif()

if(NOT TARGET game)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../game ${CMAKE_BINARY_DIR}/game_build)
endif()

set(ONLINE_SOURCES online.cpp)

if(EMSCRIPTEN)
    # In the browser, emscripten_fetch wraps the browser's own fetch().
    list(APPEND ONLINE_SOURCES http_fetch.cpp)
else()
    find_package(CURL REQUIRED)
    list(APPEND ONLINE_SOURCES http_curl.cpp)
endif()

add_library(online_lib STATIC ${ONLINE_SOURCES})

# The project root, so consumers include <giocamo/online/online.h>.
target_include_directories(online_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)

target_link_libraries(online_lib PUBLIC
    game
    nlohmann_json::nlohmann_json
)
if(NOT EMSCRIPTEN)
    target_link_libraries(online_lib PUBLIC CURL::libcurl)
endif()

target_compile_options(online_lib PRIVATE -Wall -Wextra -Wpedantic)

# Check that no message is lost, reordered, doubled or reshaped. Native only:
# it is run from a terminal. See the comment at the top of test_online.cpp.
if(NOT EMSCRIPTEN AND NOT TARGET online_test)
    add_executable(online_test test_online.cpp)
    target_link_libraries(online_test PRIVATE online_lib)
endif()
