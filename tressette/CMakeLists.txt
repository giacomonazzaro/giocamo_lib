cmake_minimum_required(VERSION 3.21)
project(tressette CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(TRESSETTE_SOURCES
    gameplay.cpp
    ai.cpp
)

# game static lib (idempotent: skipped if already added by a parent project).
if(NOT TARGET game)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../game" game_build)
endif()

# Optional: neural value-network inference via LibTorch.
# Build with:  cmake .. -DCMAKE_PREFIX_PATH=/path/to/libtorch
find_package(Torch QUIET)
if(Torch_FOUND)
    message(STATUS "LibTorch found — building with Value_Net / Agent_Minimax_Neural support")
    list(APPEND TRESSETTE_SOURCES value_net.cpp neural_agent.cpp)
endif()

# ----- Standalone C++ static library (no Python) -----
if(NOT TARGET tressette_lib)
    add_library(tressette_lib STATIC ${TRESSETTE_SOURCES})
    target_include_directories(tressette_lib PUBLIC . "${CMAKE_CURRENT_SOURCE_DIR}/..")
    target_link_libraries(tressette_lib PUBLIC game)
    if(Torch_FOUND)
        target_link_libraries(tressette_lib PUBLIC ${TORCH_LIBRARIES})
        target_include_directories(tressette_lib PUBLIC ${TORCH_INCLUDE_DIRS})
        target_compile_definitions(tressette_lib PUBLIC TORCH_AVAILABLE)
    endif()
endif()

# Headless self-play data generator. Lives here because it's a tressette-only
# tool with no graphical dependencies.
if(NOT TARGET tressette_selfplay)
    add_executable(tressette_selfplay selfplay.cpp)
    target_link_libraries(tressette_selfplay PRIVATE tressette_lib)
endif()

# Headless MCTS-vs-Minimax match-up. Plays N games, prints win counts.
if(NOT TARGET tressette_match_mcts)
    add_executable(tressette_match_mcts match_mcts.cpp)
    target_link_libraries(tressette_match_mcts PRIVATE tressette_lib)
endif()
