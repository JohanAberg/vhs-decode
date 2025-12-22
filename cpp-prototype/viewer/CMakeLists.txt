cmake_minimum_required(VERSION 3.16)

project(vhs-rf-viewer
    VERSION 0.1.0
    DESCRIPTION "Qt-based RF viewer for VHS-Decode"
    LANGUAGES CXX
)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt settings
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Widgets
    OpenGL
    OpenGLWidgets
)

# Source files
set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/videowidget.cpp
    src/timelinewidget.cpp
    src/decoderworker.cpp
    src/framecache.cpp
    src/frameexporter.cpp
)

# Header files
set(HEADERS
    include/mainwindow.h
    include/videowidget.h
    include/timelinewidget.h
    include/decoderworker.h
    include/framecache.h
    include/frameexporter.h
)

# UI files)
set(UI_FILES
    ui/mainwindow.ui
)

set_source_files_properties(${UI_FILES} PROPERTIES
    AUTOUIC_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/ui"
)

# Set UI file properties to help Qt find them
set_source_files_properties(
    ui/mainwindow.ui
    PROPERTIES
    AUTOUIC_SEARCH_PATHS ${CMAKE_CURRENT_SOURCE_DIR}/ui
)

# Create executable
add_executable(vhs-rf-viewer
    ${SOURCES}
    ${HEADERS}
    ${UI_FILES}
)

set_target_properties(vhs-rf-viewer PROPERTIES
    AUTOUIC_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/ui"
)

# Set target properties for Qt
set_target_properties(vhs-rf-viewer PROPERTIES
    AUTOUIC_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/ui"
)

# Include directories for decoder and viewer
target_include_directories(vhs-rf-viewer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# Link Qt libraries
target_link_libraries(vhs-rf-viewer PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    Threads::Threads
    opengl32  # Windows OpenGL library
)

# Note: Decoder library intentionally not linked in mock UI mode
# This keeps the viewer independent of FFTW3 and other native deps

# Platform-specific settings
if(WIN32)
    # Windows: Create GUI executable (not console)
    set_target_properties(vhs-rf-viewer PROPERTIES WIN32_EXECUTABLE TRUE)
    
    # Enable High DPI support on Windows
    target_compile_definitions(vhs-rf-viewer PRIVATE
        QT_ENABLE_HIGHDPI_SCALING
    )
elseif(APPLE)
    # macOS: Create app bundle
    set_target_properties(vhs-rf-viewer PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "org.vhs-decode.viewer"
        MACOSX_BUNDLE_BUNDLE_NAME "VHS RF Viewer"
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    )
    
    # Enable Retina display support on macOS
    set_target_properties(vhs-rf-viewer PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
    )
elseif(UNIX)
    # Linux: Standard executable
    # Enable Wayland support if available
    find_package(Qt6 QUIET COMPONENTS WaylandClient)
    if(Qt6WaylandClient_FOUND)
        target_link_libraries(vhs-rf-viewer PRIVATE Qt6::WaylandClient)
    endif()
endif()

# Installation
install(TARGETS vhs-rf-viewer
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)

# Cross-platform notes:
# - Linux: Tested on Ubuntu 22.04+, Fedora 38+, Arch Linux
# - Windows: Requires Qt6 for Windows (MSVC or MinGW builds)
# - macOS: Requires Qt6 for macOS (Clang builds), tested on macOS 12+
# 
# Build instructions per platform:
# 
# Linux:
#   sudo apt-get install qt6-base-dev libqt6opengl6-dev qt6-base-dev-tools
#   cmake -B build -S . && cmake --build build
# 
# Windows:
#   - Install Qt6 from qt.io
#   - Use Qt Creator or:
#   cmake -B build -S . -G "Visual Studio 17 2022"
#   cmake --build build --config Release
# 
# macOS:
#   brew install qt@6
#   export CMAKE_PREFIX_PATH="/opt/homebrew/opt/qt@6"
#   cmake -B build -S . && cmake --build build
