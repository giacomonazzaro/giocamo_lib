cmake_minimum_required(VERSION 3.21)
project(chess CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CHESS_SOURCES
    gameplay.cpp
    ai.cpp
)

# game static lib (idempotent: skipped if already added by a parent project).
if(NOT TARGET game)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../game" game_build)
endif()

if(NOT TARGET chess_lib)
    add_library(chess_lib STATIC ${CHESS_SOURCES})
    target_include_directories(chess_lib PUBLIC . "${CMAKE_CURRENT_SOURCE_DIR}/..")
    target_link_libraries(chess_lib PUBLIC game)
endif()

# Headless Minimax-vs-MCTS match-up. Plays N games, prints win counts and
# per-agent wall-clock totals.
if(NOT TARGET test)
    add_executable(test test.cpp)
    target_link_libraries(test PRIVATE chess_lib)
endif()

# Headless softmax-rollout MCTS vs random-rollout MCTS match-up.
if(NOT TARGET benchmark)
    add_executable(benchmark benchmark.cpp)
    target_link_libraries(benchmark PRIVATE chess_lib)
    find_package(Threads REQUIRED)
    target_link_libraries(benchmark PRIVATE Threads::Threads)
endif()

# Move-generation regression check: counts the legal-move tree from the start
# and compares against known-correct totals. Exits non-zero on any mismatch.
if(NOT TARGET move_generation_test)
    add_executable(move_generation_test move_generation_test.cpp)
    target_link_libraries(move_generation_test PRIVATE chess_lib)
endif()
