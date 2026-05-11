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

