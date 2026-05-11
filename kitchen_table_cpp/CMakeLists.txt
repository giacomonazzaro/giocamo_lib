cmake_minimum_required(VERSION 3.21)
project(kitchen_table_cpp CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(KT_SOURCES
    code/models.cpp
    code/config.cpp
    code/game_state.cpp
    code/input.cpp
    code/rendering.cpp
    code/ui.cpp
)

# ----- Standalone C++ static library (no Python) -----
# Built when the enclosing project provides a real raylib target (via
# FetchContent or find_package). The parent project must define `raylib`
# before add_subdirectory()-ing this one.
if(TARGET raylib AND NOT TARGET kitchen_table_cpp_lib)
    add_library(kitchen_table_cpp_lib STATIC ${KT_SOURCES})
    target_include_directories(kitchen_table_cpp_lib PUBLIC code)
    target_link_libraries(kitchen_table_cpp_lib PUBLIC raylib)
endif()

# ----- Python extension module (existing nanobind build) -----
# Skipped when a pure-C++ parent project sets STANDALONE_CPP_ONLY.
if(STANDALONE_CPP_ONLY)
    return()
endif()
find_package(Python 3.12 COMPONENTS Interpreter Development.Module QUIET)
if(Python_FOUND)
    execute_process(
        COMMAND "${Python_EXECUTABLE}" -c "import nanobind; print(nanobind.cmake_dir())"
        OUTPUT_VARIABLE NB_DIR OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE NB_RC
    )
    if(NB_RC EQUAL 0)
        list(APPEND CMAKE_PREFIX_PATH "${NB_DIR}")
        find_package(nanobind CONFIG QUIET)
    endif()
endif()

if(nanobind_FOUND AND NOT TARGET _kt_cpp)
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
        ${KT_SOURCES}
        code/bindings.cpp
    )
    target_compile_definitions(_kt_cpp PRIVATE KT_BUILD_PYTHON)
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
endif()
