cmake_minimum_required(VERSION 3.21)
project(kitchen_table_cpp CXX)
set(CMAKE_CXX_STANDARD 17)

find_package(Python 3.12 COMPONENTS Interpreter Development.Module REQUIRED)

# Find nanobind installed in the active Python environment.
execute_process(
    COMMAND "${Python_EXECUTABLE}" -c "import nanobind; print(nanobind.cmake_dir())"
    OUTPUT_VARIABLE NB_DIR OUTPUT_STRIP_TRAILING_WHITESPACE
)
list(APPEND CMAKE_PREFIX_PATH "${NB_DIR}")
find_package(nanobind CONFIG REQUIRED)

# Download raylib 5.5 headers for compilation only.
# Symbols resolve at runtime from pyray's already-loaded _raylib_cffi.so.
set(RAYLIB_INCLUDE_DIR "${CMAKE_BINARY_DIR}/raylib_headers")
if(NOT EXISTS "${RAYLIB_INCLUDE_DIR}/raylib.h")
    file(MAKE_DIRECTORY "${RAYLIB_INCLUDE_DIR}")
    message(STATUS "Downloading raylib 5.5 headers...")
    foreach(h raylib.h rlgl.h raymath.h)
        file(DOWNLOAD
            "https://raw.githubusercontent.com/raysan5/raylib/5.5/src/${h}"
            "${RAYLIB_INCLUDE_DIR}/${h}"
        )
    endforeach()
endif()

nanobind_add_module(_kt_cpp NB_STATIC
    src/models.cpp
    src/config.cpp
    src/game_state.cpp
    src/input.cpp
    src/rendering.cpp
    src/bindings.cpp
)
target_include_directories(_kt_cpp PRIVATE include "${RAYLIB_INCLUDE_DIR}")

# Let the linker leave raylib symbols unresolved at build time;
# pyray's _raylib_cffi.so provides them when the module is imported.
if(APPLE)
    target_link_options(_kt_cpp PRIVATE -undefined dynamic_lookup)
elseif(UNIX)
    target_link_options(_kt_cpp PRIVATE -Wl,--allow-shlib-undefined)
endif()

# Install the .so next to kitchen_table/ so `import kitchen_table._kt_cpp` works.
install(TARGETS _kt_cpp DESTINATION "${CMAKE_SOURCE_DIR}/../kitchen_table")
