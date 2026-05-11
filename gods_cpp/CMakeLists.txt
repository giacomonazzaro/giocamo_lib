cmake_minimum_required(VERSION 3.21)
project(gods_cpp CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(GODS_SOURCES
    code/models.cpp
    code/gameplay.cpp
    code/cards.cpp
    code/setup.cpp
    code/ai.cpp
)

# game_cpp static lib (idempotent: skipped if already added by a parent project).
if(NOT TARGET game_cpp)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../game_cpp" game_cpp_build)
endif()

# ----- Standalone C++ static library (no Python) -----
if(NOT TARGET gods_cpp_lib)
    add_library(gods_cpp_lib STATIC ${GODS_SOURCES})
    target_include_directories(gods_cpp_lib PUBLIC code "${CMAKE_CURRENT_SOURCE_DIR}/..")
    target_link_libraries(gods_cpp_lib PUBLIC game_cpp)
endif()

# ----- Python extension module (existing nanobind build) -----
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

if(nanobind_FOUND AND NOT TARGET _gods_cpp)
    nanobind_add_module(_gods_cpp NB_STATIC
        ${GODS_SOURCES}
        code/bindings.cpp
    )
    target_compile_definitions(_gods_cpp PRIVATE GODS_BUILD_PYTHON)
    target_link_libraries(_gods_cpp PRIVATE game_cpp)
    target_include_directories(_gods_cpp PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")

    # Install the .so next to gods/ so `import gods._gods_cpp` works.
    install(TARGETS _gods_cpp DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}/../gods")
endif()
