cmake_minimum_required(VERSION 3.15)
project(CoNES LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_definitions(NOMINMAX)
    set(CMAKE_VS_GLOBALS "IgnoreWarnIntDirInTempDetected=true")
endif()

# Find Python
find_package(Python COMPONENTS Interpreter Development.Module REQUIRED)

# Find nanobind
execute_process(
    COMMAND ${Python_EXECUTABLE} -m nanobind --cmake_dir
    OUTPUT_VARIABLE nanobind_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
list(APPEND CMAKE_PREFIX_PATH "${nanobind_DIR}")
find_package(nanobind CONFIG REQUIRED)

# Eigen and Core
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

# Python module
nanobind_add_module(cones src/bindings.cpp)

# CLI executable
add_executable(cnes src/main.cpp)
target_include_directories(cnes PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Installation targets for packaging
install(TARGETS cones DESTINATION .)
install(TARGETS cnes RUNTIME DESTINATION bin)