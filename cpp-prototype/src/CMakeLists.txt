# Decoder library source files (without main.cpp)
set(LIB_SOURCES
    formats/format_base.cpp
    formats/vhs_format.cpp
    io/rf_reader.cpp
    io/tbc_writer.cpp
    rf/fft_engine.cpp
    rf/filter_bank.cpp
    rf/hilbert.cpp
    rf/rf_processor.cpp
    demod/fm_demodulator.cpp
    sync/sync_detector.cpp
    sync/vsync_detector.cpp
    tbc/tbc_scaler.cpp
    chroma/chroma_processor.cpp
    video/dropout_corrector.cpp
    video/tbc_corrector.cpp
    core/block_processor.cpp
    core/decoder_pipeline.cpp
)

# Add FFTW3-dependent sources if available
# Note: chroma_separator and video_processor are incomplete stubs
# The main decoder (fft_engine.cpp) already uses FFTW3 when available
if(USE_FFTW3 AND FFTW3_FOUND)
    # Uncomment when these files are ready:
    # list(APPEND LIB_SOURCES 
    #     rf/fftw_engine.cpp
    #     video/chroma_separator.cpp
    #     video/video_processor.cpp
    # )
endif()

# Add OpenCL sources if available
if(ENABLE_GPU AND OpenCL_FOUND)
    list(APPEND LIB_SOURCES 
        gpu/opencl_context.cpp
        gpu/opencl_buffer.cpp
        gpu/kernel_loader.cpp
        gpu/phase_unwrap_kernel.cpp
        gpu/envelope_kernel.cpp
        gpu/filter_kernel.cpp
    )
endif()

# Add clFFT sources if available
if(USE_CLFFT AND clFFT_FOUND)
    list(APPEND LIB_SOURCES gpu/clfft_engine.cpp)
endif()

# Create decoder library
add_library(vhsdecode-lib STATIC ${LIB_SOURCES})

# Link libraries to decoder lib
target_link_libraries(vhsdecode-lib
    PUBLIC
        Threads::Threads
)

# Link FFTW3 if available
if(USE_FFTW3 AND FFTW3_FOUND)
    target_link_libraries(vhsdecode-lib PUBLIC ${FFTW3_LIBRARIES})
endif()

# Link OpenCL if available
if(ENABLE_GPU AND OpenCL_FOUND)
    target_link_libraries(vhsdecode-lib PUBLIC OpenCL::OpenCL)
endif()

# Link clFFT if available
if(USE_CLFFT AND clFFT_FOUND)
    target_link_libraries(vhsdecode-lib PUBLIC clFFT::clFFT)
endif()

# Create CLI executable
add_executable(vhs-decode-cpp core/main.cpp)

# Link decoder library to CLI
target_link_libraries(vhs-decode-cpp PRIVATE vhsdecode-lib)

# Set properties
set_target_properties(vhs-decode-cpp PROPERTIES
    OUTPUT_NAME vhs-decode
)
