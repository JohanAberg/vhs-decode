# Source files
set(SOURCES
    formats/format_base.cpp
    formats/vhs_format.cpp
    io/rf_reader.cpp
    io/tbc_writer.cpp
    rf/fft_engine.cpp
    demod/fm_demodulator.cpp
    core/main.cpp
)

# Create executable
add_executable(vhs-decode-cpp ${SOURCES})

# Link libraries
target_link_libraries(vhs-decode-cpp
    PRIVATE
        Threads::Threads
)

# Set properties
set_target_properties(vhs-decode-cpp PROPERTIES
    OUTPUT_NAME vhs-decode
)
