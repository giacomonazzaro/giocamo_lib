cmake_minimum_required(VERSION 3.15)
project(game_cpp CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(game_cpp STATIC
  game.cpp
  agent.cpp
)

target_include_directories(game_cpp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(game_cpp PRIVATE -Wall -Wextra -Wpedantic)

add_executable(tic_tac_toe tictactoe/tic_tac_toe.cpp)
target_link_libraries(tic_tac_toe PRIVATE game_cpp)
target_include_directories(tic_tac_toe PRIVATE tictactoe)
target_compile_options(tic_tac_toe PRIVATE -Wall -Wextra -Wpedantic)

add_executable(test_minimax_draws tictactoe/test_minimax_draws.cpp)
target_link_libraries(test_minimax_draws PRIVATE game_cpp)
target_include_directories(test_minimax_draws PRIVATE tictactoe)
target_compile_options(test_minimax_draws PRIVATE -Wall -Wextra -Wpedantic)

# Optional Python module. Built only when the enclosing project has already
# called find_package(nanobind) — i.e. game_cpp is being added as a
# subdirectory by a game (tressette, gods, ...) that needs the shared
# bindings. Standalone `cmake -S game_cpp` skips this target.
if(COMMAND nanobind_add_module AND NOT TARGET _game_cpp)
  nanobind_add_module(_game_cpp NB_STATIC python.cpp)
  target_link_libraries(_game_cpp PRIVATE game_cpp)
  # Install into the top-level Python `game/` package so it imports as
  # `game._game_cpp`. CMAKE_CURRENT_SOURCE_DIR is this CMakeLists' directory,
  # so the path is stable regardless of which game pulls game_cpp in.
  install(TARGETS _game_cpp DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}/../game")
endif()
