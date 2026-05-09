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

add_executable(tic_tac_toe tic_tac_toe.cpp)
target_link_libraries(tic_tac_toe PRIVATE game_cpp)
target_compile_options(tic_tac_toe PRIVATE -Wall -Wextra -Wpedantic)
