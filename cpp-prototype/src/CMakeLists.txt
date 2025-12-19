# Source files
set(SOURCES
    formats/format_base.cpp
    formats/vhs_format.cpp
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
