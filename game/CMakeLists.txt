cmake_minimum_required(VERSION 3.15)
project(game CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(game STATIC
  game.cpp
  agent.cpp
)

# The parent dir is on the path so headers can reach <basic/array.h>, which
# inlined_vector.h (pulled in by game.h) depends on.
target_include_directories(game PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_options(game PRIVATE -Wall -Wextra -Wpedantic)

# add_executable(tic_tac_toe tictactoe/tic_tac_toe.cpp)
# target_link_libraries(tic_tac_toe PRIVATE game)
# target_include_directories(tic_tac_toe PRIVATE tictactoe)
# target_compile_options(tic_tac_toe PRIVATE -Wall -Wextra -Wpedantic)

# add_executable(test_minimax_draws tictactoe/test_minimax_draws.cpp)
# target_link_libraries(test_minimax_draws PRIVATE game)
# target_include_directories(test_minimax_draws PRIVATE tictactoe)
# target_compile_options(test_minimax_draws PRIVATE -Wall -Wextra -Wpedantic)
