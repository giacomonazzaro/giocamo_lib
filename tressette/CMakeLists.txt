cmake_minimum_required(VERSION 3.21)
project(tressette_graphical CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# raylib is vendored as a sibling clone at the project root.
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../raylib ${CMAKE_BINARY_DIR}/raylib_build)

# Tell sibling libraries to skip their nanobind targets.
set(STANDALONE_CPP_ONLY ON CACHE BOOL "" FORCE)

# Header-only interface library (C++ files merged into headers).
add_library(tressette_lib INTERFACE)
target_include_directories(tressette_lib INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../game     ${CMAKE_BINARY_DIR}/game_build)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../tabletop ${CMAKE_BINARY_DIR}/kt_build)

target_link_libraries(tressette_lib INTERFACE game)

add_executable(tressette_graphical main.cpp)
target_include_directories(tressette_graphical PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/.."  # so headers can use <game/x.h>, <tressette/x.h>, etc.
)
target_link_libraries(tressette_graphical PRIVATE
    game
    tressette_lib
    tabletop_lib
    raylib
)

# Optional: neural value-network inference via LibTorch.
# Build with:  cmake .. -DCMAKE_PREFIX_PATH=/path/to/libtorch
# Download LibTorch from https://pytorch.org (select C++/Java, your OS, CPU or CUDA).
find_package(Torch QUIET)
if(Torch_FOUND)
    message(STATUS "LibTorch found — building with Value_Net / Agent_Minimax_Neural support")
    target_link_libraries(tressette_lib INTERFACE ${TORCH_LIBRARIES})
    target_include_directories(tressette_lib INTERFACE ${TORCH_INCLUDE_DIRS})
    target_compile_definitions(tressette_lib INTERFACE TORCH_AVAILABLE)
else()
    message(STATUS "LibTorch not found — value_net.h / neural_agent.h disabled (set CMAKE_PREFIX_PATH to enable)")
endif()

# Headless self-play data generator: no raylib, no tabletop. See selfplay.cpp.
add_executable(tressette_selfplay selfplay.cpp)
target_include_directories(tressette_selfplay PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
)
target_link_libraries(tressette_selfplay PRIVATE
    game
    tressette_lib
)
