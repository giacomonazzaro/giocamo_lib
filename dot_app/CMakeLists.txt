cmake_minimum_required(VERSION 3.21)
project(dot_app CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# raylib is vendored as a sibling clone at the project root.
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)
# Tell raylib to build for Web + WebGL2 when cross-compiling with Emscripten.
if(EMSCRIPTEN)
    set(PLATFORM "Web" CACHE STRING "" FORCE)
    set(GRAPHICS "GRAPHICS_API_OPENGL_ES3" CACHE STRING "" FORCE)
endif()
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../raylib ${CMAKE_BINARY_DIR}/raylib_build)

# Tell sibling libraries to skip their nanobind targets.
set(STANDALONE_CPP_ONLY ON CACHE BOOL "" FORCE)

# Pull in the sibling libraries. Each exposes a *_lib static target once
# raylib is defined by this parent project.
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../game     ${CMAKE_BINARY_DIR}/game_build)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../tabletop ${CMAKE_BINARY_DIR}/tabletop_build)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../dot      ${CMAKE_BINARY_DIR}/dot_build)

add_executable(dot_app
    main.cpp
    ui.cpp
)
target_include_directories(dot_app PRIVATE
    .
    "${CMAKE_CURRENT_SOURCE_DIR}/.."  # so headers can use <dot/x.h>, <tabletop/x.h>.
)
target_link_libraries(dot_app PRIVATE
    game
    dot_lib
    tabletop_lib
    raylib
)

if(EMSCRIPTEN)
    # Flags from the raylib Working-for-Web guide.
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -s USE_GLFW=3 -s ASSERTIONS=1 -s WASM=1 -s ASYNCIFY -s GL_ENABLE_GET_PROC_ADDRESS=1 -s FETCH=1")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s USE_GLFW=3 -s ASSERTIONS=1 -s WASM=1 -s ASYNCIFY -s GL_ENABLE_GET_PROC_ADDRESS=1 -s FETCH=1")
    set(CMAKE_EXECUTABLE_SUFFIX ".html")

    set(ASSETS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} \
        -s TOTAL_MEMORY=268435456 \
        -s MAX_WEBGL_VERSION=2 \
        -s MIN_WEBGL_VERSION=2 \
        --shell-file ${CMAKE_CURRENT_SOURCE_DIR}/../raylib/src/minshell.html \
        --preload-file ${ASSETS_ROOT}/data/fonts@/data/fonts \
        --preload-file ${ASSETS_ROOT}/tabletop/background.frag@/tabletop/background.frag \
        --preload-file ${ASSETS_ROOT}/tabletop/data/wood.png@/tabletop/data/wood.png")
endif()
