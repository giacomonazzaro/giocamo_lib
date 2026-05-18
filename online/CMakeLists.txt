cmake_minimum_required(VERSION 3.21)
project(online CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(CURL REQUIRED)

set(ONLINE_SOURCES
    protocol.cpp
    setup.cpp
    agents.cpp
)

# nlohmann_json is vendored as a sibling clone at the project root. Guard so we
# don't re-add it when a parent project has already done so.
if(NOT TARGET nlohmann_json::nlohmann_json)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../nlohmann_json ${CMAKE_BINARY_DIR}/nlohmann_json_build)
endif()

# game (for Agent base class). Idempotent: skipped if a parent already added it.
if(NOT TARGET game)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../game ${CMAKE_BINARY_DIR}/game_build)
endif()

# ----- Standalone C++ static library -----
if(NOT TARGET online_lib)
    add_library(online_lib STATIC ${ONLINE_SOURCES})
    # PUBLIC '..' so consumers can include <online/agents.h>.
    target_include_directories(online_lib PUBLIC . ${CMAKE_CURRENT_SOURCE_DIR}/..)
    target_link_libraries(online_lib PUBLIC CURL::libcurl nlohmann_json::nlohmann_json game)
endif()

