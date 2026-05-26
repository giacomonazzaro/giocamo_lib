cmake_minimum_required(VERSION 3.21)
project(online CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# nlohmann_json is vendored as a sibling clone at the project root. Guard so we
# don't re-add it when a parent project has already done so.
if(NOT TARGET nlohmann_json::nlohmann_json)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../nlohmann_json ${CMAKE_BINARY_DIR}/nlohmann_json_build)
endif()

# game (for Agent base class). Idempotent: skipped if a parent already added it.
if(NOT TARGET game)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../game ${CMAKE_BINARY_DIR}/game_build)
endif()

set(ONLINE_SOURCES
    protocol.cpp
    setup.cpp
    agents.cpp
)

if(EMSCRIPTEN)
    # Browser transport: emscripten_fetch wraps the browser's fetch() API.
    list(APPEND ONLINE_SOURCES http_fetch.cpp)
else()
    # Native transport: libcurl for HTTP, same as the original online code.
    find_package(CURL REQUIRED)
    list(APPEND ONLINE_SOURCES http_curl.cpp)
endif()

if(NOT TARGET online_lib)
    add_library(online_lib STATIC ${ONLINE_SOURCES})
    target_include_directories(online_lib PUBLIC . ${CMAKE_CURRENT_SOURCE_DIR}/..)
    target_link_libraries(online_lib PUBLIC nlohmann_json::nlohmann_json game)
    if(NOT EMSCRIPTEN)
        target_link_libraries(online_lib PUBLIC CURL::libcurl)
    endif()
endif()

# Headless smoke test: two peers in one process via ntfy.sh, scripted
# agents, trivial alternating-turn game. Native-only — wasm doesn't run
# from CLI.
if(NOT EMSCRIPTEN AND NOT TARGET online_test_online)
    add_executable(online_test_online test_online.cpp)
    target_link_libraries(online_test_online PRIVATE online_lib)
endif()
