cmake_minimum_required(VERSION 3.21)
project(mindbug_app CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# raylib is a sibling clone at the project root.
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

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../game     ${CMAKE_BINARY_DIR}/game_build)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../tabletop ${CMAKE_BINARY_DIR}/tabletop_build)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../mindbug  ${CMAKE_BINARY_DIR}/mindbug_build)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../online   ${CMAKE_BINARY_DIR}/online_build)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../giocamo  ${CMAKE_BINARY_DIR}/giocamo_build)

add_executable(mindbug_app
    main.cpp
    ui.cpp
    agent_ui.cpp
)
target_include_directories(mindbug_app PRIVATE
    .
    "${CMAKE_CURRENT_SOURCE_DIR}/.."  # so headers can use <mindbug/x.h>, <tabletop/x.h>.
)
target_link_libraries(mindbug_app PRIVATE
    game
    mindbug_lib
    tabletop_lib
    raylib
    giocamo
    online_lib
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
        --preload-file ${ASSETS_ROOT}/data/fonts/Cinzel/Cinzel-VariableFont_wght.ttf@/data/fonts/Cinzel/Cinzel-VariableFont_wght.ttf \
        --preload-file ${ASSETS_ROOT}/mindbug/cards.json@/mindbug/cards.json \
        --preload-file ${ASSETS_ROOT}/mindbug/card-images@/mindbug/card-images \
        --preload-file ${ASSETS_ROOT}/tabletop/background.frag@/tabletop/background.frag \
        --preload-file ${ASSETS_ROOT}/tabletop/data/wood.png@/tabletop/data/wood.png")
endif()
