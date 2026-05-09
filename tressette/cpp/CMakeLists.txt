cmake_minimum_required(VERSION 3.21)
project(tressette_cpp CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Python 3.12 COMPONENTS Interpreter Development.Module REQUIRED)

# Find nanobind installed in the active Python environment.
execute_process(
    COMMAND "${Python_EXECUTABLE}" -c "import nanobind; print(nanobind.cmake_dir())"
    OUTPUT_VARIABLE NB_DIR OUTPUT_STRIP_TRAILING_WHITESPACE
)
list(APPEND CMAKE_PREFIX_PATH "${NB_DIR}")
find_package(nanobind CONFIG REQUIRED)

# Reuse the existing game_cpp static library — no source files duplicated.
# game_cpp lives two levels up (../../game_cpp).
add_subdirectory("${CMAKE_SOURCE_DIR}/../../game_cpp" game_cpp_build)

nanobind_add_module(_tressette_cpp NB_STATIC
    code/models.cpp
    code/gameplay.cpp
    code/ai.cpp
    code/bindings.cpp
)
target_link_libraries(_tressette_cpp PRIVATE game_cpp)
# Include the parent of game_cpp so headers can be referenced as <game_cpp/x.h>.
target_include_directories(_tressette_cpp PRIVATE "${CMAKE_SOURCE_DIR}/../..")

# Install the .so next to the python game shim so `import tressette.game._tressette_cpp` works.
install(TARGETS _tressette_cpp DESTINATION "${CMAKE_SOURCE_DIR}/../game")
