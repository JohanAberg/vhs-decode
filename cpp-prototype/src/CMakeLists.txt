# Source files
set(SOURCES
    formats/format_base.cpp
    formats/vhs_format.cpp
    io/rf_reader.cpp
    io/tbc_writer.cpp
    rf/fft_engine.cpp
    rf/filter_bank.cpp
    rf/hilbert.cpp
    rf/rf_processor.cpp
    demod/fm_demodulator.cpp
    video/chroma_separator.cpp
    video/dropout_corrector.cpp
    video/tbc_corrector.cpp
    video/video_processor.cpp
    core/block_processor.cpp
    core/main.cpp
)

# Add FFTW3 sources if available
if(USE_FFTW3 AND FFTW3_FOUND)
    list(APPEND SOURCES rf/fftw_engine.cpp)
endif()

# Create executable
add_executable(vhs-decode-cpp ${SOURCES})

# Link libraries
target_link_libraries(vhs-decode-cpp
    PRIVATE
        Threads::Threads
)

# Link FFTW3 if available
if(USE_FFTW3 AND FFTW3_FOUND)
    target_link_libraries(vhs-decode-cpp PRIVATE FFTW3::fftw3)
endif()

# Set properties
set_target_properties(vhs-decode-cpp PROPERTIES
    OUTPUT_NAME vhs-decode
)
