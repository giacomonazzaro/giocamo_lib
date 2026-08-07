cmake_minimum_required(VERSION 3.21)
project(tabletop CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(TABLETOP_SOURCES
    config.cpp
    tabletop.cpp
    input_recorder.cpp
    rendering.cpp
    editor.cpp
    ui.cpp
)

# When built as the top-level project, pull in raylib ourselves. A parent
# project has already defined the raylib target before adding this directory.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../raylib ${CMAKE_BINARY_DIR}/raylib_build)
endif()

if(TARGET raylib AND NOT TARGET tabletop_lib)
    # Dear ImGui, plus the raylib glue that draws its output through rlgl.
    # Both are sibling clones at the project root, like raylib.
    set(IMGUI_DIR   "${CMAKE_CURRENT_SOURCE_DIR}/../imgui")
    set(RLIMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../rlImGui")
    add_library(imgui STATIC
        "${IMGUI_DIR}/imgui.cpp"
        "${IMGUI_DIR}/imgui_draw.cpp"
        "${IMGUI_DIR}/imgui_tables.cpp"
        "${IMGUI_DIR}/imgui_widgets.cpp"
        "${RLIMGUI_DIR}/rlImGui.cpp"
    )
    target_include_directories(imgui PUBLIC "${IMGUI_DIR}" "${RLIMGUI_DIR}")
    target_link_libraries(imgui PUBLIC raylib)

    add_library(tabletop_lib STATIC ${TABLETOP_SOURCES})
    # Add parent dir so we can include <struct/json.h> for input recording.
    target_include_directories(tabletop_lib PUBLIC . "${CMAKE_CURRENT_SOURCE_DIR}/..")
    target_link_libraries(tabletop_lib PUBLIC raylib imgui)
endif()

# When built as the top-level project, also compile the demo executable.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_executable(main main.cpp)
    target_link_libraries(main PRIVATE tabletop_lib)
endif()
