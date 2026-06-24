cmake_minimum_required(VERSION 3.15)
project(CoNES LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_definitions(NOMINMAX)
    set(CMAKE_VS_GLOBALS "IgnoreWarnIntDirInTempDetected=true")
endif()

option(CONES_BUILD_PYTHON "Build Python bindings" ON)

if(CONES_BUILD_PYTHON)
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
endif()

# Eigen and Core
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Eigen")
    message(STATUS "Eigen not found locally. Fetching Eigen 5.0.0...")
    include(FetchContent)
    FetchContent_Declare(
      eigen
      URL https://gitlab.com/libeigen/eigen/-/archive/5.0.0/eigen-5.0.0.zip
    )
    FetchContent_MakeAvailable(eigen)
    include_directories(${eigen_SOURCE_DIR})
endif()
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

# Python module
if(CONES_BUILD_PYTHON)
    nanobind_add_module(_cones src/bindings.cpp)
    install(TARGETS _cones DESTINATION cones)
endif()

# CLI executable
add_executable(cnes src/main.cpp)
target_include_directories(cnes PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Installation targets for packaging
install(TARGETS cnes RUNTIME DESTINATION bin)

if(CONES_BUILD_PYTHON)
    install(DIRECTORY tools DESTINATION cones)
    install(DIRECTORY cones_studio DESTINATION cones)
    install(DIRECTORY materials DESTINATION cones OPTIONAL)
endif()