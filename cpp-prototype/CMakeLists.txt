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
option(USE_CLFFT "Use clFFT for GPU-accelerated FFT" ON)

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
    # Try pkg-config first (works on Linux)
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(FFTW3 fftw3)
    endif()
    
    # Fallback to CONFIG mode (vcpkg on Windows)
    if(NOT FFTW3_FOUND)
        find_package(FFTW3f CONFIG QUIET)
        if(TARGET FFTW3::fftw3f)
            message(STATUS "FFTW3 found via CONFIG")
            set(FFTW3_LIBRARIES FFTW3::fftw3f)
            set(FFTW3_FOUND TRUE)
        endif()
    endif()
    
    if(FFTW3_FOUND)
        message(STATUS "FFTW3 found: version ${FFTW3_VERSION}")
        message(STATUS "  Include: ${FFTW3_INCLUDE_DIRS}")
        message(STATUS "  Library: ${FFTW3_LIBRARIES}")
        add_definitions(-DHAVE_FFTW3)
        include_directories(${FFTW3_INCLUDE_DIRS})
    else()
        message(WARNING "FFTW3 not found - using prototype FFT (much slower)")
        message(STATUS "Install FFTW3: sudo apt-get install libfftw3-dev")
        set(USE_FFTW3 OFF)
    endif()
endif()

# OpenCL and clFFT (for GPU acceleration)
if(ENABLE_GPU)
    find_package(OpenCL)
    if(OpenCL_FOUND)
        message(STATUS "OpenCL found: ${OpenCL_VERSION_STRING}")
        add_definitions(-DHAVE_OPENCL)
        
        # Try to find clFFT if GPU FFT is enabled
        if(USE_CLFFT)
            find_package(clFFT)
            if(clFFT_FOUND)
                message(STATUS "clFFT found")
                add_definitions(-DHAVE_CLFFT)
            else()
                message(WARNING "clFFT not found - GPU FFT disabled")
                set(USE_CLFFT OFF)
            endif()
        endif()
    else()
        message(WARNING "OpenCL not found - GPU acceleration disabled")
        set(ENABLE_GPU OFF)
        set(USE_CLFFT OFF)
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
message(STATUS "OpenCL support: ${ENABLE_GPU}")
message(STATUS "clFFT support: ${USE_CLFFT}")
message(STATUS "Build tools: ${BUILD_TOOLS}")
message(STATUS "======================")
