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

# Find Qt6 or Qt5
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Widgets
    OpenGL
    OpenGLWidgets
)

# Cross-platform Qt deployment helpers (Windows/macOS only)

# Source files
set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/videowidget.cpp
    src/timelinewidget.cpp
    src/decoderworker.cpp
    src/framecache.cpp
    src/frameexporter.cpp
    src/histogramwidget.cpp
    src/vectorscopewidget.cpp
    src/colorsamplerwidget.cpp
    src/scanlineplotwidget.cpp
    src/spectrogramwidget.cpp
    resources.qrc
)

# Header files
set(HEADERS
    include/mainwindow.h
    include/videowidget.h
    include/timelinewidget.h
    include/decoderworker.h
    include/framecache.h
    include/frameexporter.h
    include/histogramwidget.h
    include/vectorscopewidget.h
    include/colorsamplerwidget.h
    include/scanlineplotwidget.h
    include/spectrogramwidget.h
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

# Link against decoder core + Qt dependencies
target_link_libraries(vhs-rf-viewer PRIVATE
    vhsdecode-lib
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    Threads::Threads
)
# Platform-specific settings
if(WIN32)
    # Windows: Create GUI executable (not console)
    set_target_properties(vhs-rf-viewer PROPERTIES WIN32_EXECUTABLE TRUE)

    # Embed application icon if .ico exists at configure time
    set(VIEWER_ICO "${CMAKE_CURRENT_SOURCE_DIR}/../../assets/images/viewer/signal_color_v01.ico")
    if(EXISTS "${VIEWER_ICO}")
        target_sources(vhs-rf-viewer PRIVATE viewer_icon.rc)
        message(STATUS "Embedding viewer icon: ${VIEWER_ICO}")
    else()
        message(WARNING "Viewer .ico not found at configure: ${VIEWER_ICO}. Generate via scripts/generate_viewer_icon.ps1 or use runtime Qt icon.")
    endif()
    
    # Enable High DPI support on Windows
    target_compile_definitions(vhs-rf-viewer PRIVATE
        QT_ENABLE_HIGHDPI_SCALING
    )

    target_link_libraries(vhs-rf-viewer PRIVATE opengl32)
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

# Windows: run windeployqt to copy Qt DLLs and plugins next to the exe
if(WIN32)
    # Pre-link: ensure any running instance is terminated to avoid LNK1104 (file in use)
    add_custom_command(TARGET vhs-rf-viewer PRE_LINK
        COMMAND cmd /c "taskkill /IM vhs-rf-viewer.exe /F >NUL 2>&1 || ver >NUL"
        COMMENT "Ensuring vhs-rf-viewer.exe is not running before link"
        VERBATIM)

    # Try to locate windeployqt from common vcpkg Qt6 locations
    find_program(WINDEPLOYQT_EXECUTABLE NAMES windeployqt windeployqt6
        HINTS
            "${CMAKE_PREFIX_PATH}/../tools/Qt6/bin"
            "${CMAKE_PREFIX_PATH}/../tools/qt6-base"
            "${CMAKE_PREFIX_PATH}/../tools/qt6-base/bin"
            "${CMAKE_PREFIX_PATH}/../tools/Qt6"
        NO_DEFAULT_PATH
    )

    if(NOT WINDEPLOYQT_EXECUTABLE)
        # Fallback to PATH or typical vcpkg layout
        find_program(WINDEPLOYQT_EXECUTABLE NAMES windeployqt windeployqt6)
    endif()

    # Hardcoded relative path fallback for vcpkg layout within this repo
    if(NOT WINDEPLOYQT_EXECUTABLE)
        set(_VCPKG_WINDEPLOY "${CMAKE_CURRENT_SOURCE_DIR}/../../vcpkg_installed/x64-windows/tools/Qt6/bin/windeployqt.exe")
        if(EXISTS "${_VCPKG_WINDEPLOY}")
            set(WINDEPLOYQT_EXECUTABLE "${_VCPKG_WINDEPLOY}")
        endif()
    endif()

    if(WINDEPLOYQT_EXECUTABLE)
        # Skip windeployqt for Debug builds to avoid missing Qt debug DLLs in some vcpkg setups
        # For multi-config generators, use a CMD guard on CONFIG
        add_custom_command(TARGET vhs-rf-viewer POST_BUILD
            COMMAND cmd /c "if /I \"$<CONFIG>\"==\"Debug\" (echo Skipping windeployqt for Debug) else (\"${WINDEPLOYQT_EXECUTABLE}\" \"$<TARGET_FILE:vhs-rf-viewer>\" --verbose 0)"
            COMMENT "Deploying Qt runtime (skips in Debug)"
            VERBATIM)
    else()
        message(WARNING "windeployqt not found; Qt runtime DLLs will not be auto-deployed.")
    endif()
endif()

# macOS: run macdeployqt on the app bundle
if(APPLE)
    find_program(MACDEPLOYQT_EXECUTABLE macdeployqt)
    if(MACDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET vhs-rf-viewer POST_BUILD
            COMMAND "${MACDEPLOYQT_EXECUTABLE}" "$<TARGET_FILE:vhs-rf-viewer>" -always-overwrite -verbose=0
            COMMENT "Running macdeployqt to deploy Qt runtime"
        )
    else()
        message(WARNING "macdeployqt not found; Qt runtime will not be auto-deployed.")
    endif()
endif()

# Linux: typically use system Qt; packaging is handled by distro or CPack
if(UNIX AND NOT APPLE)
    message(STATUS "Qt runtime deployment on Linux is not performed automatically.")
endif()

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
