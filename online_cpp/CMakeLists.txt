cmake_minimum_required(VERSION 3.21)
project(online_cpp CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(CURL REQUIRED)

set(ONLINE_SOURCES
    code/protocol.cpp
    code/setup.cpp
)

# nlohmann_json is required for both builds. If the parent project hasn't
# provided it, fetch it here.
if(NOT TARGET nlohmann_json::nlohmann_json)
    include(FetchContent)
    FetchContent_Declare(
        nlohmann_json
        URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

# ----- Standalone C++ static library -----
if(NOT TARGET online_cpp_lib)
    add_library(online_cpp_lib STATIC ${ONLINE_SOURCES})
    target_include_directories(online_cpp_lib PUBLIC code)
    target_link_libraries(online_cpp_lib PUBLIC CURL::libcurl nlohmann_json::nlohmann_json)
endif()

# ----- Python extension module -----
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

if(nanobind_FOUND AND NOT TARGET _online_cpp)
    nanobind_add_module(_online_cpp NB_STATIC
        ${ONLINE_SOURCES}
        code/bindings.cpp
    )
    target_compile_definitions(_online_cpp PRIVATE ONLINE_BUILD_PYTHON)
    target_link_libraries(_online_cpp PRIVATE CURL::libcurl nlohmann_json::nlohmann_json)

    install(TARGETS _online_cpp DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}/../online")
endif()
