cmake_minimum_required(VERSION 3.16)

project(vhs-decode-cpp
    VERSION 0.1.0
    DESCRIPTION "Native C++ VHS decoder with OpenCL acceleration"
    LANGUAGES CXX
)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -pedantic)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-O3)
    endif()
endif()

# Options
option(BUILD_TESTS "Build unit tests" ON)
option(BUILD_TOOLS "Build benchmark and utility tools" ON)
option(ENABLE_GPU "Enable OpenCL GPU acceleration" ON)
option(USE_FFTW3 "Use FFTW3 for production FFT (highly recommended)" ON)

# Module path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Find dependencies
find_package(Threads REQUIRED)

# FFTW3 (optional but highly recommended)
if(USE_FFTW3)
    find_package(FFTW3f CONFIG QUIET)
    if(TARGET FFTW3::fftw3f)
        message(STATUS "FFTW3 found via CONFIG")
        add_definitions(-DHAVE_FFTW3)
        set(FFTW3_LIBRARIES FFTW3::fftw3f)
        set(FFTW3_FOUND TRUE)
    else()
        message(WARNING "FFTW3 not found - using prototype FFT (much slower)")
        message(STATUS "Install FFTW3 via vcpkg: vcpkg install fftw3:x64-windows")
        set(USE_FFTW3 OFF)
    endif()
endif()

# Subdirectories
add_subdirectory(src)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(BUILD_TOOLS)
    add_subdirectory(tools)
endif()

# Print configuration
message(STATUS "")
message(STATUS "=== VHS-Decode C++ ===")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "FFTW3 support: ${USE_FFTW3}")
message(STATUS "GPU support: ${ENABLE_GPU}")
message(STATUS "Build tools: ${BUILD_TOOLS}")
message(STATUS "======================")
