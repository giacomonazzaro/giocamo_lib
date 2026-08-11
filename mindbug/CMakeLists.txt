cmake_minimum_required(VERSION 3.21)
project(mindbug CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(MINDBUG_SOURCES
    cards.cpp
    gameplay.cpp
)

# game static lib (idempotent: skipped if already added by a parent project).
if(NOT TARGET game)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../game" game_build)
endif()

# cards.json is read with nlohmann::json.
if(NOT TARGET nlohmann_json)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../nlohmann_json" nlohmann_json_build)
endif()

if(NOT TARGET mindbug_lib)
    add_library(mindbug_lib STATIC ${MINDBUG_SOURCES})
    target_include_directories(mindbug_lib PUBLIC . "${CMAKE_CURRENT_SOURCE_DIR}/..")
    target_link_libraries(mindbug_lib PUBLIC game nlohmann_json::nlohmann_json)
endif()

# Times the search on fixed positions, for comparing changes to it.
if(NOT TARGET mindbug_benchmark)
    add_executable(mindbug_benchmark benchmark.cpp)
    target_link_libraries(mindbug_benchmark PRIVATE mindbug_lib)
endif()

# Rules checks plus a batch of random games, to catch a rule that stops the
# game from ever ending.
if(NOT TARGET mindbug_test)
    add_executable(mindbug_test test.cpp)
    target_link_libraries(mindbug_test PRIVATE mindbug_lib)
endif()
