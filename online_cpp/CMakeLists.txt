cmake_minimum_required(VERSION 3.21)
project(online_cpp CXX)
set(CMAKE_CXX_STANDARD 17)

find_package(Python 3.12 COMPONENTS Interpreter Development.Module REQUIRED)

# Find nanobind installed in the active Python environment.
execute_process(
    COMMAND "${Python_EXECUTABLE}" -c "import nanobind; print(nanobind.cmake_dir())"
    OUTPUT_VARIABLE NB_DIR OUTPUT_STRIP_TRAILING_WHITESPACE
)
list(APPEND CMAKE_PREFIX_PATH "${NB_DIR}")
find_package(nanobind CONFIG REQUIRED)

find_package(CURL REQUIRED)

nanobind_add_module(_online_cpp NB_STATIC
    code/protocol.cpp
    code/setup.cpp
    code/bindings.cpp
)
target_link_libraries(_online_cpp PRIVATE CURL::libcurl)

# Install next to online/ so `import online._online_cpp` works.
install(TARGETS _online_cpp DESTINATION "${CMAKE_SOURCE_DIR}/../online")
