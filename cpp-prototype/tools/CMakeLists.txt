# Benchmark tool (disabled - needs updating for new API)
if(FALSE AND USE_FFTW3 AND FFTW3_FOUND)
    add_executable(benchmark-fft
        benchmark_fft.cpp
        ../src/rf/fft_engine.cpp
        ../src/rf/fftw_engine.cpp
    )
    
    target_include_directories(benchmark-fft PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )
    
    target_link_libraries(benchmark-fft
        PRIVATE
            ${FFTW3_LIBRARIES}
            Threads::Threads
    )
    
    message(STATUS "Building FFT benchmark tool")
else()
    message(STATUS "FFT benchmark tool requires FFTW3 - skipping")
endif()
