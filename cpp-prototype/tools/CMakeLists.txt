# Benchmark tool
if(USE_FFTW3 AND FFTW3_FOUND)
    set(BENCHMARK_SOURCES
        benchmark_fft.cpp
        ../src/rf/fft_engine.cpp
        ../src/rf/fftw_engine.cpp
    )
    
    # Add GPU sources if available
    if(ENABLE_GPU AND OpenCL_FOUND)
        list(APPEND BENCHMARK_SOURCES
            ../src/gpu/opencl_context.cpp
            ../src/gpu/opencl_buffer.cpp
        )
    endif()
    
    # Add clFFT sources if available
    if(USE_CLFFT AND clFFT_FOUND)
        list(APPEND BENCHMARK_SOURCES ../src/gpu/clfft_engine.cpp)
    endif()

    # Locate double-precision FFTW3 if available via vcpkg
    if(NOT DEFINED FFTW3_DOUBLE_LIB)
        if(EXISTS "C:/vcpkg/installed/x64-windows/lib/fftw3.lib")
            set(FFTW3_DOUBLE_LIB "C:/vcpkg/installed/x64-windows/lib/fftw3.lib")
        endif()
    endif()
    
    add_executable(benchmark-fft ${BENCHMARK_SOURCES})
    
    target_include_directories(benchmark-fft PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )
    
    target_link_libraries(benchmark-fft
        PRIVATE
            ${FFTW3_LIBRARIES}
            ${FFTW3_DOUBLE_LIB}
            Threads::Threads
    )
    
    # Link OpenCL if available
    if(ENABLE_GPU AND OpenCL_FOUND)
        target_link_libraries(benchmark-fft PRIVATE OpenCL::OpenCL)
    endif()
    
    # Link clFFT if available
    if(USE_CLFFT AND clFFT_FOUND)
        target_link_libraries(benchmark-fft PRIVATE clFFT::clFFT)
    endif()
    
    message(STATUS "Building FFT benchmark tool with GPU support: ${USE_CLFFT}")
else()
    message(STATUS "FFT benchmark tool requires FFTW3 - skipping")
endif()
